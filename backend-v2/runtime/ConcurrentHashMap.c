#include "Object.h"
#include "ConcurrentHashMap.h"
#include "Hash.h"
#include <stdlib.h>
#include <stdatomic.h>
#include "Nil.h"
#include "RTValue.h"
#include "word.h"
#include <math.h>

extern uword_t tryReservingEmptyNode(ConcurrentHashMapEntry *entry, RTValue key, RTValue value, uword_t keyHash);
extern bool tryReplacingEntry(ConcurrentHashMapEntry *entry, RTValue key, RTValue value, uword_t keyHash, uword_t encounteredHash);
extern bool tryDeletingFromEntry(ConcurrentHashMapEntry *entry, RTValue key, uword_t keyHash);
extern RTValue tryGettingFromEntry(ConcurrentHashMapEntry *entry, RTValue key, uword_t keyHash);
extern uword_t avalanche(uword_t h);
extern unsigned char getLongLeap(unsigned int leaps);
extern unsigned char getShortLeap(unsigned int leaps);
extern unsigned int buildLeaps(unsigned char longLeap, unsigned char shortLeap);

/* outside refcount system */
inline unsigned char getLongLeap(unsigned int leaps) {
  return leaps >> 8;
}

/* outside refcount system */
inline unsigned char getShortLeap(unsigned int leaps) {
  return leaps & 0xff;
}

/* outside refcount system */
inline unsigned int buildLeaps(unsigned char longLeap, unsigned char shortLeap) {
  return ((unsigned short)longLeap) << 8 | ((unsigned short) shortLeap);
}


/* mem done */
ConcurrentHashMap *ConcurrentHashMap_create(unsigned char initialSizeExponent) {
  size_t initialSize = 1<<initialSizeExponent;
  ConcurrentHashMap *self = allocate(sizeof(ConcurrentHashMap)); 
  size_t rootSize = sizeof(ConcurrentHashMapNode) + initialSize * sizeof(ConcurrentHashMapEntry);
  ConcurrentHashMapNode *node = allocate(rootSize);
  memset(node, 0, rootSize);
  node->sizeMask = initialSize - 1ull;
  node->resizingThreshold = MIN(round(pow(initialSize, 0.33)), 128);
  for (uword_t i = 0; i < initialSize; i++) {
    atomic_store_explicit(&(node->array[i].key), RT_boxNull(),
                          memory_order_relaxed);
    atomic_store_explicit(&(node->array[i].value), RT_boxNull(), memory_order_relaxed);    
  }    
  atomic_store(&(self->root), node);              
  Object_create(&(self->super), concurrentHashMapType);
  return self;
}

/* outside refcount system */
inline uword_t tryReservingEmptyNode(ConcurrentHashMapEntry *entry, RTValue key, RTValue value, uword_t keyHash) { 
  /* Returns 0 if reservation is successful, otherwise the encountered hash */
  while(true) {
    uword_t encounteredHash =
        atomic_load_explicit(&(entry->keyHash), memory_order_relaxed);
    if(encounteredHash == 0) {
      if(atomic_compare_exchange_strong_explicit(&(entry->keyHash), &encounteredHash, keyHash, memory_order_relaxed, memory_order_relaxed)) {
        atomic_store_explicit(&(entry->key), key, memory_order_relaxed);
        atomic_store_explicit(&(entry->value), value, memory_order_relaxed);
        return 0;
      }
      continue;
    }
    return encounteredHash;  
  }
}

/* outside refcount system */
inline bool tryReplacingEntry(ConcurrentHashMapEntry *entry, RTValue key, RTValue value, uword_t keyHash, uword_t encounteredHash) {
  RTValue encounteredKey = RT_boxNull();
  /* Returns true if successfully replaced, otherwise false */
  if(encounteredHash == keyHash) {
    while(RT_isNull(encounteredKey = atomic_load_explicit(&(entry->key), memory_order_relaxed)));
    if(equals(encounteredKey, key)) {
      while(true) {
        RTValue encounteredValue = atomic_load_explicit(&(entry->value), memory_order_relaxed);
        if (atomic_compare_exchange_strong_explicit(
                &(entry->value), &encounteredValue, value, memory_order_relaxed,
                memory_order_relaxed)) {
          // for null autorelease does nothing
          autorelease(encounteredValue);
          release(key);
          return true;
        } 
      }
    }
  }
  return false;
}

/* MUTABLE: self is not conusmed. mem done */
void ConcurrentHashMap_assoc(ConcurrentHashMap *self, RTValue key, RTValue value) {
  ConcurrentHashMapNode *root = atomic_load_explicit(&(self->root), memory_order_relaxed);

  uword_t keyHash = avalanche(hash(key));
  uword_t startIndex = keyHash & root->sizeMask;
  uword_t index = startIndex;
  ConcurrentHashMapEntry *entry = &(root->array[index]);

  //printf("keyHash: %u, startindex: %u index: %u\n", keyHash, startIndex, index); 
  
  
  uword_t encounteredHash = tryReservingEmptyNode(entry, key, value, keyHash);
  if(encounteredHash == 0) { //printf("STORED AT: %d %u\n", index, RT_unboxInt32(key)); 
    return; 
  }
  if (tryReplacingEntry(entry, key, value, keyHash, encounteredHash)) {
//    printf("REPLACED AT: %u\n", index);     
    return;
  }
  unsigned short lastChainEntryLeaps = atomic_load_explicit(&(entry->leaps), memory_order_relaxed);
  unsigned short jump = getLongLeap(lastChainEntryLeaps);

  while(jump > 0) { /* Let us traverse the chain searching for its end */
    index = (index + jump) & root->sizeMask;
    entry = &(root->array[index]);
    /* We wait until the node appears */
    while((encounteredHash = atomic_load_explicit(&(entry->keyHash), memory_order_relaxed)) == 0); 
    /* Maybe it is an existing node with the key? If so we replace the value */
    if (tryReplacingEntry(entry, key, value, keyHash, encounteredHash)) {
  //    printf("REPLACED2 AT: %u\n", index);           
      return; 
    }
    lastChainEntryLeaps = atomic_load_explicit(&(entry->leaps), memory_order_relaxed);
    jump = getShortLeap(lastChainEntryLeaps);    
  }

  ConcurrentHashMapEntry *lastChainEntry = entry;
  uword_t lastChainEntryIndex = index;
  bool lastChainEntryIsFirstEntry = lastChainEntryIndex == startIndex;

  /* We enter linear scan */
  for (unsigned char i = 1; i < root->resizingThreshold; i++) {
//    printf("Linear scan %d\n", i);
    index = (index + 1) & root->sizeMask;
    entry = &(root->array[index]);
    encounteredHash = tryReservingEmptyNode(entry, key, value, keyHash);

    while (encounteredHash == 0) {
  //        printf("success %d\n", index);     
      /* We succeeded in reserving the node! We create long or short jumps. */
      unsigned short newLeaps = lastChainEntryIsFirstEntry ? buildLeaps(i, getShortLeap(lastChainEntryLeaps)) : buildLeaps(getLongLeap(lastChainEntryLeaps), i);
      if(atomic_compare_exchange_strong_explicit(&(lastChainEntry->leaps), &lastChainEntryLeaps, newLeaps, memory_order_relaxed, memory_order_relaxed)) {
        return;
      } 
      lastChainEntryLeaps = atomic_load_explicit(&(lastChainEntry->leaps), memory_order_relaxed);
    }

    while((encounteredHash & root->sizeMask)  == startIndex) {
      /* This is a problem. Another thread is in a process of adding an entry to the same chain! 
         We need to patch the last chain entry for him */      
      unsigned short newLeaps = lastChainEntryIsFirstEntry ? buildLeaps(i, getShortLeap(lastChainEntryLeaps)) : buildLeaps(getLongLeap(lastChainEntryLeaps), i);
      if(atomic_compare_exchange_strong_explicit(&(lastChainEntry->leaps), &lastChainEntryLeaps, newLeaps, memory_order_relaxed, memory_order_relaxed)) {
        /* We move the last entry index and start afresh */
        lastChainEntryIndex = index;
        lastChainEntry = entry;
        lastChainEntryLeaps = atomic_load_explicit(&(lastChainEntry->leaps), memory_order_relaxed);
        lastChainEntryIsFirstEntry = false;
        i = 0;
        break;
      } 
      lastChainEntryLeaps = atomic_load_explicit(&(lastChainEntry->leaps), memory_order_relaxed);
    }
  }
  /* The loop failed - the table is overcrowded and needs a migration - TODO + releases */
  printf("keyHash: %lu, startindex: %lu endChainIndex: %lu lastIndex: %lu\n", keyHash, startIndex, lastChainEntryIndex, index); 
  release(key);
  release(value);
  assert(false && "Overcrowded - resizing not yet supported");  
}

/* outside refcount system */
inline bool tryDeletingFromEntry(ConcurrentHashMapEntry *entry, RTValue key, uword_t keyHash) {
  uword_t encounteredHash = atomic_load_explicit(&(entry->keyHash), memory_order_relaxed);
  RTValue encounteredKey = RT_boxNull();
  if(encounteredHash == 0) { 
    release(key);
    return true;  
  }
  if(encounteredHash == keyHash) { 
    /* This is a weak dissoc, still leaves dangling key in the table */
    encounteredKey = atomic_load_explicit(&(entry->key), memory_order_relaxed);
    /* sbd is in the process of insertion, but we ignore it as dissoc works only for what 
       was already present in the table */
    if(RT_isNull(key)) { 
      return true;
    }
    if(equals(encounteredKey, key)) {
      RTValue encounteredValue = atomic_load_explicit(&(entry->value), memory_order_relaxed);
      atomic_exchange(&(entry->value), RT_boxNull());
      autorelease(encounteredValue);
      release(key);
      return true;
    }
  }
  return false;
}

/* MUTABLE: self is not conusmed. mem done */
void ConcurrentHashMap_dissoc(ConcurrentHashMap *self, RTValue key) {
  ConcurrentHashMapNode *root = atomic_load_explicit(&(self->root), memory_order_relaxed);
  uword_t keyHash = avalanche(hash(key));
  uword_t startIndex = keyHash & root->sizeMask;
  uword_t index = startIndex;
  ConcurrentHashMapEntry *entry = &(root->array[index]);
  
  if(tryDeletingFromEntry(entry, key, keyHash)) { return; }

  unsigned short jump = getLongLeap(atomic_load_explicit(&(entry->leaps), memory_order_relaxed));
  while(jump > 0) {
    index = (index + jump) & root->sizeMask;
    entry = &(root->array[index]);
    if(tryDeletingFromEntry(entry, key, keyHash)) { return; }
    jump = getShortLeap(atomic_load_explicit(&(entry->leaps), memory_order_relaxed));  
  }
  release(key);
}

/* outside refcount system */
inline RTValue tryGettingFromEntry(ConcurrentHashMapEntry *entry, RTValue key, uword_t keyHash) {
  /* NULL return value signifies failure */
  uword_t encounteredHash = atomic_load_explicit(&(entry->keyHash), memory_order_relaxed);
  RTValue encounteredKey = RT_boxNull();
  
  if(encounteredHash == 0) { return RT_boxNil(); }
  if(encounteredHash == keyHash) {
    encounteredKey = atomic_load_explicit(&(entry->key), memory_order_relaxed);
    if(!encounteredKey) { return RT_boxNil(); }
    if(equals(encounteredKey, key)) {
      RTValue value = atomic_load_explicit(&(entry->value), memory_order_relaxed);
      if(!RT_isNull(value)) { 
        retain(value);        
        return value;
      } 
      return RT_boxNil(); 
    }
  }
  return RT_boxNull();
}

/* MUTABLE: self is not conusmed, keyV is consumed. mem done */
RTValue ConcurrentHashMap_get(ConcurrentHashMap *self, RTValue key) {
  ConcurrentHashMapNode *root = atomic_load_explicit(&(self->root), memory_order_relaxed);
  uword_t keyHash = avalanche(hash(key));
  uword_t startIndex = keyHash & root->sizeMask;
  uword_t index = startIndex;
  ConcurrentHashMapEntry *entry = &(root->array[index]);
  
  RTValue retVal = tryGettingFromEntry(entry, key, keyHash);
 
  if(!RT_isNull(retVal)) {
    release(key); 
    return retVal;
  }
  unsigned short jump = getLongLeap(atomic_load_explicit(&(entry->leaps), memory_order_relaxed));  
  while(jump > 0) {
    index = (index + jump) & root->sizeMask;
    entry = &(root->array[index]);
    retVal = tryGettingFromEntry(entry, key, keyHash);
    if(!RT_isNull(retVal)) {
      release(key); 
      return retVal;
    }
    
    jump = getShortLeap(atomic_load_explicit(&(entry->leaps), memory_order_relaxed));  
  }

  release(key);
  return RT_boxNil();  
}

/* outside refcount system */
bool ConcurrentHashMap_equals(ConcurrentHashMap *self, ConcurrentHashMap *other) {
  /* CHM is equal only on pointer basis */
  /* Warning!!! 
     The key-value pairs would have to be stably ordered to get this function.
     We leave it out for now - as we do not expect this data structure to be used anywhere
     outside global ref registers. (and so the hash function is actually redundant) */

  return false;
}

/* outside refcount system */
uword_t ConcurrentHashMap_hash(ConcurrentHashMap *self) {
  /* CHM has unique hash equal to its pointer */
  /* Warning!!! 
     The key-value pairs would have to be stably ordered to get this function.
     We leave it out for now - as we do not expect this data structure to be used anywhere
     outside global ref registers. (and so the hash function is actually redundant) */

  return (uword_t) self;
}

/* String *ConcurrentHashMap_toDebugString(ConcurrentHashMap *self) { */
/*   ConcurrentHashMapNode *root = atomic_load_explicit(&(self->root), memory_order_relaxed); */
/*   sds retVal = sdsnew("{"); */
/*   bool found = false; */
/*   for(uword_t i=0; i<= root->sizeMask; i++) { */
/*     ConcurrentHashMapEntry *entry = &(root->array[i]); */
/*     RTValue key = entry->key; */
/*     RTValue value = entry->value; */
/*     if(key && value) { */
/*       if(found) retVal = sdscat(retVal, ", "); */
/*       found = true;       */
/*       String *ks = toString(key); */
/*       String *vs = toString(value); */
/*       retVal = sdscatprintf(retVal, "Index: %d; hash %lu; Good index: %lu, leap %d:%d --> ", i, entry->keyHash ,  entry->keyHash & root->sizeMask, getLongLeap(entry->leaps), getShortLeap(entry->leaps)); */
/*       retVal = sdscatsds(retVal, ks->value); */
/*       retVal = sdscat(retVal, " "); */
/*       retVal = sdscatsds(retVal, vs->value); */
/*       retVal = sdscat(retVal, "\n"); */
/*       release(ks); */
/*       release(vs); */
/*     } else { */
/*       retVal = sdscatprintf(retVal, "Index: %d; empty\n", i); */
/*     } */
/*   } */
/*   retVal = sdscat(retVal, "}"); */
/*   return String_create(retVal); */
/* } */

/* mem done */
String *ConcurrentHashMap_toString(ConcurrentHashMap *self) {
  ConcurrentHashMapNode *root = atomic_load_explicit(&(self->root), memory_order_relaxed);
  String *retVal = String_create("{");
  String *comma = String_create(", ");
  String *space = String_create(" "); 
  String *closing = String_create("}"); 

  bool found = false;
  for(uword_t i=0; i<= root->sizeMask; i++) {
    ConcurrentHashMapEntry *entry = &(root->array[i]);
    RTValue key = entry->key;
    RTValue value = entry->value;
    if(!RT_isNull(key) && !RT_isNull(value)) {
      if(found) { 
        Ptr_retain(comma);
        retVal = String_concat(retVal, comma);
      }
      found = true;      
      retain(key);
      retain(value);
      String *ks = toString(key);
      String *vs = toString(value);
      Ptr_retain(space);
      retVal = String_concat(retVal, ks);
      retVal = String_concat(retVal, space);
      retVal = String_concat(retVal, vs);
    } 
  }
  retVal = String_concat(retVal, closing);
  Ptr_release(comma);
  Ptr_release(space);
  Ptr_release(closing);
  Ptr_release(self);
  return retVal;
}


/* outside refcount system */
void ConcurrentHashMap_destroy(ConcurrentHashMap *self) {
    ConcurrentHashMapNode *root = atomic_load_explicit(&(self->root), memory_order_relaxed);
    atomic_store(&(self->root), NULL);
    for(uword_t i=0; i<= root->sizeMask; i++) {
      ConcurrentHashMapEntry *entry = &(root->array[i]);
      RTValue key = atomic_load_explicit(&(entry->key), memory_order_relaxed);
      RTValue value =
          atomic_load_explicit(&(entry->value), memory_order_relaxed);
      // The special case of 0 is a double which does not need any release, so its fine.
      release(key);
      release(value);
    }
    deallocate(root);
}

