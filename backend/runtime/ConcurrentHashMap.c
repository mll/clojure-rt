#include "Object.h"
#include "ConcurrentHashMap.h"
#include "Hash.h"
#include <stdlib.h>
#include <stdatomic.h>
#include "Nil.h"
#include "sds/sds.h"
#include <math.h>

extern Nil *UNIQUE_NIL;

extern uint64_t tryReservingEmptyNode(ConcurrentHashMapEntry *entry, Object *key, Object *value, uint64_t keyHash);
extern BOOL tryReplacingEntry(ConcurrentHashMapEntry *entry, Object *key, Object *value, uint64_t keyHash, uint64_t encounteredHash);
extern BOOL tryDeletingFromEntry(ConcurrentHashMapEntry *entry, Object *key, uint64_t keyHash);
extern Object *tryGettingFromEntry(ConcurrentHashMapEntry *entry, Object *key, uint64_t keyHash);
extern uint64_t avalanche_64(uint64_t h);

ConcurrentHashMap *ConcurrentHashMap_create(unsigned char initialSizeExponent) {
  size_t initialSize = 1<<initialSizeExponent;
  Object *super = allocate(sizeof(ConcurrentHashMap) + sizeof(Object)); 
  ConcurrentHashMap *self = (ConcurrentHashMap *)(super + 1);
  size_t rootSize = sizeof(ConcurrentHashMapNode) + initialSize * sizeof(ConcurrentHashMapEntry);
  ConcurrentHashMapNode *node = allocate(rootSize);
  memset(node, 0, rootSize);
  node->sizeMask = initialSize - 1ull;
  node->resizingThreshold = MIN(round(pow(initialSize, 0.33)), 128);
  atomic_store(&(self->root), node);              
  Object_create(super, concurrentHashMapType);
  return self;
}

inline unsigned char getLongLeap(unsigned int leaps) {
  return leaps >> 8;
}

inline unsigned char getShortLeap(unsigned int leaps) {
  return leaps & 0xff;
}

inline unsigned int buildLeaps(unsigned char longLeap, unsigned char shortLeap) {
  return ((unsigned short)longLeap) << 8 | ((unsigned short) shortLeap);
}

inline uint64_t tryReservingEmptyNode(ConcurrentHashMapEntry *entry, Object *key, Object *value, uint64_t keyHash) { 
  /* Returns 0 if reservation is successful, otherwise the encountered hash */
  while(TRUE) {
    uint64_t encounteredHash = atomic_load_explicit(&(entry->keyHash), memory_order_relaxed);
    if(encounteredHash == 0) {
      if(atomic_compare_exchange_strong_explicit(&(entry->keyHash), &encounteredHash, keyHash, memory_order_relaxed, memory_order_relaxed)) {
        atomic_store(&(entry->key), key);
        atomic_store(&(entry->value), value);
        Object_retain(key);
        Object_retain(value);
        return 0;
      }
      continue;
    }
    return encounteredHash;  
  }
}

inline BOOL tryReplacingEntry(ConcurrentHashMapEntry *entry, Object *key, Object *value, uint64_t keyHash, uint64_t encounteredHash) {
  Object* encounteredKey = NULL;
  /* Returns TRUE if successfully replaced, otherwise FALSE */
  if(encounteredHash == keyHash) {
    while((encounteredKey = atomic_load_explicit(&(entry->key), memory_order_relaxed)) == NULL);
    if(equals(encounteredKey, key)) {
      while(TRUE) {
        Object *encounteredValue = atomic_load_explicit(&(entry->value), memory_order_relaxed);
        if(atomic_compare_exchange_strong_explicit(&(entry->value), &encounteredValue, value, memory_order_relaxed, memory_order_relaxed)) {
          if(encounteredValue) Object_autorelease(encounteredValue);
          Object_retain(value);
          return TRUE;
        } 
      }
    }
  }
  return FALSE;
}

/* Memory model - swallows both */
void ConcurrentHashMap_assoc(ConcurrentHashMap *self, Object *key, Object *value) {
  ConcurrentHashMapNode *root = atomic_load_explicit(&(self->root), memory_order_relaxed);
  uint64_t keyHash = avalanche_64(hash(key));
  uint64_t startIndex = keyHash & root->sizeMask;
  uint64_t index = startIndex;
  ConcurrentHashMapEntry *entry = &(root->array[index]);

//  printf("keyHash: %u, startindex: %u index: %u\n", keyHash, startIndex, index); 
  
  uint64_t encounteredHash = tryReservingEmptyNode(entry, key, value, keyHash);
  if(encounteredHash == 0) { //printf("STORED AT: %u\n", index); 
    return; }
  if(tryReplacingEntry(entry, key, value, keyHash, encounteredHash)) return;

  unsigned short lastChainEntryLeaps = atomic_load_explicit(&(entry->leaps), memory_order_relaxed);
  unsigned short jump = getLongLeap(lastChainEntryLeaps);

  while(jump > 0) { /* Let us traverse the chain searching for its end */
    index = (index + jump) & root->sizeMask;
    entry = &(root->array[index]);
    /* We wait until the node appears */
    while((encounteredHash = atomic_load_explicit(&(entry->keyHash), memory_order_relaxed)) == 0); 
    /* Maybe it is an existing node with the key? If so we replace the value */
    if(tryReplacingEntry(entry, key, value, keyHash, encounteredHash)) return; 
    lastChainEntryLeaps = atomic_load_explicit(&(entry->leaps), memory_order_relaxed);
    jump = getShortLeap(lastChainEntryLeaps);    
  }

  ConcurrentHashMapEntry *lastChainEntry = entry;
  uint64_t lastChainEntryIndex = index;
  BOOL lastChainEntryIsFirstEntry = lastChainEntryIndex == startIndex;

  /* We enter linear scan */
  for(unsigned char i=1; i<root->resizingThreshold; i++) {
    index = (index + 1) & root->sizeMask;
    entry = &(root->array[index]);
    encounteredHash = tryReservingEmptyNode(entry, key, value, keyHash);
  retry_cas:
    if(encounteredHash == 0) {
      /* We succeeded in reserving the node! We create long or short jumps. */
      unsigned short newLeaps = lastChainEntryIsFirstEntry ? buildLeaps(i, getShortLeap(lastChainEntryLeaps)) : buildLeaps(getLongLeap(lastChainEntryLeaps), i);
      if(atomic_compare_exchange_strong_explicit(&(lastChainEntry->leaps), &lastChainEntryLeaps, newLeaps, memory_order_relaxed, memory_order_relaxed)) {
        return;
      } else {
        lastChainEntryLeaps = atomic_load_explicit(&(lastChainEntry->leaps), memory_order_relaxed);
        goto retry_cas;
      }
    }

  retry_cas_2:            
    if((encounteredHash & root->sizeMask)  == startIndex) {
      /* This is a problem. Another thread is in a process of adding an entry to the same chain! 
         We need to patch the last chain entry for him */      
      unsigned short newLeaps = lastChainEntryIsFirstEntry ? buildLeaps(i, getShortLeap(lastChainEntryLeaps)) : buildLeaps(getLongLeap(lastChainEntryLeaps), i);
      if(atomic_compare_exchange_strong_explicit(&(lastChainEntry->leaps), &lastChainEntryLeaps, newLeaps, memory_order_relaxed, memory_order_relaxed)) {
        /* We move the last entry index and start afresh */
        lastChainEntryIndex = index;
        lastChainEntry = entry;
        lastChainEntryLeaps = atomic_load_explicit(&(lastChainEntry->leaps), memory_order_relaxed);
        lastChainEntryIsFirstEntry = FALSE;
        i = 0;
      } else {
        lastChainEntryLeaps = atomic_load_explicit(&(lastChainEntry->leaps), memory_order_relaxed);
        goto retry_cas_2;        
      }
    }
  }
  /* The loop failed - the table is overcrowded and needs a migration - TODO + releases */
  printf("keyHash: %llu, startindex: %llu endChainIndex: %llu lastIndex: %llu\n", keyHash, startIndex, lastChainEntryIndex, index); 
  
  assert(FALSE && "Overcrowded - resizing not yet supported");  
}

inline BOOL tryDeletingFromEntry(ConcurrentHashMapEntry *entry, Object *key, uint64_t keyHash) {
  uint64_t encounteredHash = atomic_load_explicit(&(entry->keyHash), memory_order_relaxed);
  Object *encounteredKey = NULL;
  if(encounteredHash == 0) return TRUE;  
  if(encounteredHash == keyHash) { 
    /* This is a weak dissoc, still leaves dangling key in the table */
    encounteredKey = atomic_load_explicit(&(entry->key), memory_order_relaxed);
    /* sbd is in the process of insertion, but we ignore it as dissoc works only for what 
       was already present in the table */
    if(!encounteredKey) return TRUE;
    if(equals(encounteredKey, key)) {
      Object *encounteredValue = atomic_load_explicit(&(entry->value), memory_order_relaxed);
      atomic_exchange(&(entry->value), NULL);
      if(encounteredValue)  Object_autorelease(encounteredValue);
      return TRUE;
    }
  }
  return FALSE;
}

void ConcurrentHashMap_dissoc(ConcurrentHashMap *self, Object *key) {
  ConcurrentHashMapNode *root = atomic_load_explicit(&(self->root), memory_order_relaxed);
  uint64_t keyHash = avalanche_64(hash(key));
  uint64_t startIndex = keyHash & root->sizeMask;
  uint64_t index = startIndex;
  ConcurrentHashMapEntry *entry = &(root->array[index]);
  
  if(tryDeletingFromEntry(entry, key, keyHash)) return;

  unsigned short jump = getLongLeap(atomic_load_explicit(&(entry->leaps), memory_order_relaxed));
  while(jump > 0) {
    index = (index + jump) & root->sizeMask;
    entry = &(root->array[index]);
    if(tryDeletingFromEntry(entry, key, keyHash)) return;
    jump = getShortLeap(atomic_load_explicit(&(entry->leaps), memory_order_relaxed));  
  }
}


inline Object *tryGettingFromEntry(ConcurrentHashMapEntry *entry, Object *key, uint64_t keyHash) {
  /* NULL return value signifies failure */
  uint64_t encounteredHash = atomic_load_explicit(&(entry->keyHash), memory_order_relaxed);
  Object *encounteredKey = NULL;
  
  if(encounteredHash == 0) { retain(UNIQUE_NIL); return super(UNIQUE_NIL); }
  if(encounteredHash == keyHash) {
    encounteredKey = atomic_load_explicit(&(entry->key), memory_order_relaxed);
    if(!encounteredKey) { retain(UNIQUE_NIL); return super(UNIQUE_NIL); }
    if(equals(encounteredKey, key)) {
      Object *value = atomic_load_explicit(&(entry->value), memory_order_relaxed);
      if(value) { 
        Object_retain(value);        
        return value;
      }
      retain(UNIQUE_NIL); 
      return super(UNIQUE_NIL); 
    }
  }
  return NULL;
}


Object *ConcurrentHashMap_get(ConcurrentHashMap *self, Object *key) {
  ConcurrentHashMapNode *root = atomic_load_explicit(&(self->root), memory_order_relaxed);
  uint64_t keyHash = avalanche_64(hash(key));
  uint64_t startIndex = keyHash & root->sizeMask;
  uint64_t index = startIndex;
  ConcurrentHashMapEntry *entry = &(root->array[index]);
  
  Object *retVal = tryGettingFromEntry(entry, key, keyHash);
 
  if(retVal) return retVal;

  unsigned short jump = getLongLeap(atomic_load_explicit(&(entry->leaps), memory_order_relaxed));  
  while(jump > 0) {
    index = (index + jump) & root->sizeMask;
    entry = &(root->array[index]);
    retVal = tryGettingFromEntry(entry, key, keyHash);
    if(retVal) return retVal;
    jump = getShortLeap(atomic_load_explicit(&(entry->leaps), memory_order_relaxed));  
  }
  
  retain(UNIQUE_NIL); 
  return super(UNIQUE_NIL); 
}

BOOL ConcurrentHashMap_equals(ConcurrentHashMap *self, ConcurrentHashMap *other) {
  assert(FALSE && "Concurrent hash map does not implement equals");
  /* Warning!!! 
     The key-value pairs would have to be stably ordered to get this function.
     We leave it out for now - as we do not expect this data structure to be used anywhere
     outside global ref registers. (and so the hash function is actually redundant */

  return FALSE;
}

uint64_t ConcurrentHashMap_hash(ConcurrentHashMap *self) {
  assert(FALSE && "Concurrent hash map does not implement hash");
  /* Warning!!! 
     The key-value pairs would have to be stably ordered to get this function.
     We leave it out for now - as we do not expect this data structure to be used anywhere
     outside global ref registers. (and so the hash function is actually redundant */

  return 5381;
}

String *ConcurrentHashMap_toString(ConcurrentHashMap *self) {
  ConcurrentHashMapNode *root = atomic_load_explicit(&(self->root), memory_order_relaxed);
  sds retVal = sdsnew("{");
  BOOL found = FALSE;
  for(int i=0; i<= root->sizeMask; i++) {
    ConcurrentHashMapEntry *entry = &(root->array[i]);
    Object *key = entry->key;
    Object *value = entry->value;
    if(key && value) {
      if(found) retVal = sdscat(retVal, ", ");
      found = TRUE;      
      String *ks = toString(key);
      String *vs = toString(value);
      retVal = sdscatprintf(retVal, "Index: %d; hash %llu; Good index: %llu, leap %d:%d --> ", i, entry->keyHash ,  entry->keyHash & root->sizeMask, getLongLeap(entry->leaps), getShortLeap(entry->leaps));
      retVal = sdscatsds(retVal, ks->value);
      retVal = sdscat(retVal, " ");
      retVal = sdscatsds(retVal, vs->value);
      retVal = sdscat(retVal, "\n");
      release(ks);
      release(vs);
    } else {
      retVal = sdscatprintf(retVal, "Index: %d; empty\n", i);
    }
  }
  retVal = sdscat(retVal, "}");
  return String_create(retVal);
}

void ConcurrentHashMap_destroy(ConcurrentHashMap *self) {
    ConcurrentHashMapNode *root = atomic_load_explicit(&(self->root), memory_order_relaxed);
    atomic_store(&(self->root), NULL);
    for(int i=0; i<= root->sizeMask; i++) {
      ConcurrentHashMapEntry *entry = &(root->array[i]);
      Object *key = atomic_load_explicit(&(entry->key), memory_order_relaxed);
      Object *value = atomic_load_explicit(&(entry->value), memory_order_relaxed);
      if(key) Object_release(key);
      if(value) Object_release(value);
    }
    deallocate(root);
}
