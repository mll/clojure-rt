#include "ConcurrentHashMap.h"
#include "Ebr.h"
#include "Exceptions.h"
#include "Object.h"
#include "RTValue.h"
#include "tests/TestTools.h"
#include "word.h"
#include <math.h>
#include <stdatomic.h>
#include <stdlib.h>

/**************** HELPERS ****************/

#define LINEAR_SEARCH_LIMIT 128

/* outside refcount system */
static RTValue tryGettingFromEntry(ConcurrentHashMapEntry *entry, RTValue key,
                                   uword_t keyHash) {
  if (atomic_load_explicit(&(entry->keyHash), memory_order_relaxed) !=
      keyHash) {
    return RT_boxNull();
  }

  RTValue encounteredKey = RT_boxNull();
  while (RT_isNull(encounteredKey = atomic_load_explicit(
                       &(entry->key), memory_order_acquire))) {
#if defined(__x86_64__) || defined(_M_X64)
    __builtin_ia32_pause();
#endif
  }

  if (equals(encounteredKey, key)) {
    RTValue value = atomic_load_explicit(&(entry->value), memory_order_acquire);
    retain(value);
    return value;
  }

  return RT_boxNull();
}

/* outside refcount system */
static bool tryDeletingFromEntry(ConcurrentHashMapEntry *entry, RTValue key,
                                 uword_t keyHash) {
  uword_t encounteredHash =
      atomic_load_explicit(&(entry->keyHash), memory_order_relaxed);
  RTValue encounteredKey = RT_boxNull();
  if (encounteredHash == 0) {
    release(key);
    return true;
  }
  if (encounteredHash == keyHash) {
    /* This is a weak dissoc, still leaves dangling key in the table */
    while (RT_isNull(encounteredKey = atomic_load_explicit(
                         &(entry->key), memory_order_acquire)))
      ;
    /* sbd is in the process of insertion, but we ignore it as dissoc works only
       for what was already present in the table */
    if (equals(encounteredKey, key)) {
      RTValue encounteredValue = atomic_exchange_explicit(
          &(entry->value), RT_boxNull(), memory_order_acq_rel);

      autorelease(encounteredValue);

      release(key);
      return true;
    }
  }
  return false;
}

/* outside memory management */
static bool isSameKey(ConcurrentHashMapEntry *entry, RTValue key) {
  RTValue encounteredKey;
  while (RT_isNull(encounteredKey = atomic_load_explicit(&(entry->key),
                                                         memory_order_relaxed)))
    ;
  return equals(encounteredKey, key);
}

enum ConcurrentHashMapInsertResult { AlreadyFound, InsertedNew, Overflow };
/* outside memory management */
static enum ConcurrentHashMapInsertResult
insertOrFind(ConcurrentHashMapNode *root, uword_t keyHash, RTValue key,
             ConcurrentHashMapEntry **cell, uword_t *overflowIdx) {

  uword_t sizeMask = root->sizeMask;
  uword_t idx = keyHash;

  *cell = &(root->array[idx & sizeMask]);

  uword_t probeHash =
      atomic_load_explicit(&((*cell)->keyHash), memory_order_relaxed);
  if (probeHash == 0) {
    if (atomic_compare_exchange_strong_explicit(&((*cell)->keyHash), &probeHash,
                                                keyHash, memory_order_relaxed,
                                                memory_order_relaxed)) {
      return InsertedNew;
    }
  }

  if (probeHash == keyHash && isSameKey(*cell, key)) {
    return AlreadyFound;
  }

  bool useShortLeap = false;
  _Atomic(unsigned char) *prevLink = NULL;
  for (;;) {
  followLink:
    prevLink = useShortLeap ? &(*cell)->shortLeap : &(*cell)->longLeap;
    unsigned char probeDelta =
        atomic_load_explicit(prevLink, memory_order_relaxed);
    useShortLeap = true;
    if (probeDelta > 0) {
      idx += probeDelta;
      *cell = &(root->array[idx & sizeMask]);
      probeHash =
          atomic_load_explicit(&((*cell)->keyHash), memory_order_relaxed);
      if (probeHash == 0) {
        // Cell was linked, but hash is not visible yet.
        do {
          probeHash =
              atomic_load_explicit(&((*cell)->keyHash), memory_order_acquire);
        } while (probeHash == 0);
      }
      if (probeHash == keyHash && isSameKey(*cell, key)) {
        return AlreadyFound;
      }
    } else {
      // Reached the end of the link chain for this bucket.
      uword_t prevLinkIdx = idx;
      uword_t linearProbesRemaining = root->resizingThreshold;
      while (linearProbesRemaining-- > 0) {
        idx++;
        *cell = &(root->array[idx & sizeMask]);
        probeHash =
            atomic_load_explicit(&((*cell)->keyHash), memory_order_relaxed);
        if (probeHash == 0) {
          if (atomic_compare_exchange_strong_explicit(
                  &((*cell)->keyHash), &probeHash, keyHash,
                  memory_order_relaxed, memory_order_relaxed)) {
            uword_t desiredDelta = idx - prevLinkIdx;
            atomic_store_explicit(prevLink, desiredDelta, memory_order_relaxed);
            return InsertedNew;
          }
        }

        uword_t x = probeHash ^ keyHash;

        if (!x && isSameKey(*cell, key)) {
          return AlreadyFound;
        }

        if ((x & sizeMask) == 0) {
          uword_t desiredDelta = idx - prevLinkIdx;
          atomic_store_explicit(prevLink, desiredDelta, memory_order_relaxed);
          goto followLink;
        }
      }
      *overflowIdx = idx + 1;
      return Overflow;
    }
  }
}

/**************** /HELPERS ****************/

/* mem done */
ConcurrentHashMap *ConcurrentHashMap_create(unsigned char initialSizeExponent) {
  size_t initialSize = 1 << initialSizeExponent;
  ConcurrentHashMap *self = allocate(sizeof(ConcurrentHashMap));
  size_t rootSize = sizeof(ConcurrentHashMapNode) +
                    initialSize * sizeof(ConcurrentHashMapEntry);
  ConcurrentHashMapNode *node = allocate(rootSize);
  Object_create(&(node->super), concurrentHashMapNodeType);
  // Concurrent collections are shared by definition
  atomic_store_explicit(&node->super.atomicRefCount, COUNT_INC | SHARED_BIT,
                        memory_order_relaxed);
  node->sizeMask = initialSize - 1ull;
  node->resizingThreshold = MIN(round(pow(initialSize, 0.33)), 128);
  atomic_init(&node->next, NULL);
  atomic_init(&node->migrationIndex, 0);
  atomic_init(&node->count, 0);

  for (uword_t i = 0; i < initialSize; i++) {
    ConcurrentHashMapEntry *entry = &(node->array[i]);
    atomic_store_explicit(&(entry->key), RT_boxNull(), memory_order_relaxed);
    atomic_store_explicit(&(entry->value), RT_boxNull(), memory_order_relaxed);
    atomic_store_explicit(&(entry->keyHash), 0, memory_order_relaxed);
    atomic_store_explicit(&(entry->shortLeap), 0, memory_order_relaxed);
    atomic_store_explicit(&(entry->longLeap), 0, memory_order_relaxed);
  }
  atomic_store(&(self->root), node);
  Object_create(&(self->super), concurrentHashMapType);
  // Concurrent collections are shared by definition
  atomic_store_explicit(&self->super.atomicRefCount, COUNT_INC | SHARED_BIT,
                        memory_order_release);
  return self;
}

/* mem done */
void ConcurrentHashMap_assoc(ConcurrentHashMap *self, RTValue key,
                             RTValue value) {
  ConcurrentHashMap_assoc_preservesSelf(self, key, value, true);
  Ptr_release(self);
}

/* self is not conusmed. mem done */
void ConcurrentHashMap_assoc_preservesSelf(ConcurrentHashMap *self, RTValue key,
                                           RTValue value,
                                           bool releaseSelfOnError) {
  promoteToShared(key);
  promoteToShared(value);

  ConcurrentHashMapNode *root =
      atomic_load_explicit(&(self->root), memory_order_relaxed);
  uword_t keyHash = hash(key);
  if (keyHash == 0)
    keyHash = 0x1;

  ConcurrentHashMapEntry *cell;
  uword_t overflowIdx;
  enum ConcurrentHashMapInsertResult result =
      insertOrFind(root, keyHash, key, &cell, &overflowIdx);
  switch (result) {
  case AlreadyFound: {
    RTValue oldVal =
        atomic_exchange_explicit(&(cell->value), value, memory_order_acq_rel);
    autorelease(oldVal);
    release(key);
    break;
  }
  case InsertedNew:
    atomic_store_explicit(&(cell->value), value, memory_order_relaxed);
    atomic_store_explicit(&(cell->key), key, memory_order_release);
    break;
  case Overflow:
    release(key);
    release(value);
    if (releaseSelfOnError) {
      Ptr_release(self);
    }
    throwIllegalStateException_C(
        "ConcurrentHashMap_assoc - overcrowded - resizing not yet supported");

    break;
  }
}
/* self is not consumed, mem done */
RTValue ConcurrentHashMap_getOrCreate_preservesSelf(ConcurrentHashMap *self,
                                                    RTValue key, RTValue value,
                                                    bool releaseSelfOnError) {
  retain(value);
  RTValue retVal = ConcurrentHashMap_putIfAbsent_preservesSelf(
      self, key, value, releaseSelfOnError);
  if (RT_isNil(retVal)) {
    return value;
  }
  release(value);
  return retVal;
}

/* self is not consumed, mem done */
RTValue ConcurrentHashMap_putIfAbsent_preservesSelf(ConcurrentHashMap *self,
                                                    RTValue key, RTValue value,
                                                    bool releaseSelfOnError) {
  promoteToShared(key);
  promoteToShared(value);

  ConcurrentHashMapNode *root =
      atomic_load_explicit(&(self->root), memory_order_relaxed);
  uword_t keyHash = hash(key);
  if (keyHash == 0)
    keyHash = 0x1;

  ConcurrentHashMapEntry *cell;
  uword_t overflowIdx;
  enum ConcurrentHashMapInsertResult result =
      insertOrFind(root, keyHash, key, &cell, &overflowIdx);
  switch (result) {
  case AlreadyFound: {
    RTValue oldVal = RT_boxNull();
    release(key);
    if (atomic_compare_exchange_strong_explicit(&(cell->value), &oldVal, value,
                                                memory_order_release,
                                                memory_order_acquire)) {
      return RT_boxNil();
    }

    release(value);
    retain(oldVal);
    return oldVal;
  }
  case InsertedNew:
    atomic_store_explicit(&(cell->value), value, memory_order_relaxed);
    atomic_store_explicit(&(cell->key), key, memory_order_release);
    return RT_boxNil();
    break;
  case Overflow:
    release(key);
    release(value);
    if (releaseSelfOnError) {
      Ptr_release(self);
    }

    throwIllegalStateException_C("ConcurrentHashMap_putIfAbsent - overcrowded "
                                 "- resizing not yet supported");

    break;
  }
}

/* mem done */
RTValue ConcurrentHashMap_putIfAbsent(ConcurrentHashMap *self, RTValue key,
                                      RTValue value) {
  RTValue retVal =
      ConcurrentHashMap_putIfAbsent_preservesSelf(self, key, value, true);
  Ptr_release(self);
  return retVal;
}

/* mem done */
RTValue ConcurrentHashMap_getOrCreate(ConcurrentHashMap *self, RTValue key,
                                      RTValue value) {
  RTValue retVal =
      ConcurrentHashMap_getOrCreate_preservesSelf(self, key, value, true);
  Ptr_release(self);
  return retVal;
}

/* mem done */
void ConcurrentHashMap_dissoc(ConcurrentHashMap *self, RTValue key) {
  ConcurrentHashMap_dissoc_preservesSelf(self, key);
  Ptr_release(self);
}

/* self is not consumed, mem done */
void ConcurrentHashMap_dissoc_preservesSelf(ConcurrentHashMap *self,
                                            RTValue key) {
  ConcurrentHashMapNode *root =
      atomic_load_explicit(&(self->root), memory_order_relaxed);
  uword_t keyHash = hash(key);
  if (keyHash == 0)
    keyHash = 0x1;
  uword_t startIndex = keyHash & root->sizeMask;
  uword_t index = startIndex;
  ConcurrentHashMapEntry *entry = &(root->array[index]);

  if (tryDeletingFromEntry(entry, key, keyHash)) {
    return;
  }

  unsigned short jump =
      atomic_load_explicit(&(entry->longLeap), memory_order_relaxed);
  while (jump > 0) {
    index = (index + jump) & root->sizeMask;
    entry = &(root->array[index]);
    if (tryDeletingFromEntry(entry, key, keyHash)) {
      return;
    }
    jump = atomic_load_explicit(&(entry->shortLeap), memory_order_relaxed);
  }
  release(key);
}

/* mem done */
RTValue ConcurrentHashMap_get(ConcurrentHashMap *self, RTValue key) {
  RTValue retVal = ConcurrentHashMap_get_preservesSelf(self, key);
  Ptr_release(self);
  return retVal;
}

/* self is not conusmed, mem done */
RTValue ConcurrentHashMap_get_preservesSelf(ConcurrentHashMap *self,
                                            RTValue key) {
  ConcurrentHashMapNode *root =
      atomic_load_explicit(&(self->root), memory_order_relaxed);
  uword_t keyHash = hash(key);
  if (keyHash == 0)
    keyHash = 0x1;
  uword_t startIndex = keyHash & root->sizeMask;
  uword_t index = startIndex;
  ConcurrentHashMapEntry *entry = &(root->array[index]);

  RTValue retVal = tryGettingFromEntry(entry, key, keyHash);

  if (!RT_isNull(retVal)) {
    release(key);
    return retVal;
  }

  unsigned char jump =
      atomic_load_explicit(&(entry->longLeap), memory_order_relaxed);

  while (jump > 0) {
    index = (index + jump) & root->sizeMask;
    entry = &(root->array[index]);
    retVal = tryGettingFromEntry(entry, key, keyHash);
    if (!RT_isNull(retVal)) {
      release(key);
      return retVal;
    }

    jump = atomic_load_explicit(&(entry->shortLeap), memory_order_relaxed);
  }

  release(key);
  return RT_boxNil();
}

/* outside refcount system */
bool ConcurrentHashMap_equals(ConcurrentHashMap *self,
                              ConcurrentHashMap *other) {
  /* CHM is equal only on pointer basis */
  /* Warning!!!
     The key-value pairs would have to be stably ordered to get this function.
     We leave it out for now - as we do not expect this data structure to be
     used anywhere outside global ref registers. (and so the hash function is
     actually redundant) */

  return false;
}

/* outside refcount system */
uword_t ConcurrentHashMap_hash(ConcurrentHashMap *self) {
  /* CHM has unique hash equal to its pointer */
  /* Warning!!!
     The key-value pairs would have to be stably ordered to get this function.
     We leave it out for now - as we do not expect this data structure to be
     used anywhere outside global ref registers. (and so the hash function is
     actually redundant) */

  return (uword_t)self;
}

/* mem done */
String *ConcurrentHashMap_toString(ConcurrentHashMap *self) {
  ConcurrentHashMapNode *root =
      atomic_load_explicit(&(self->root), memory_order_relaxed);
  String *retVal = String_create("{");
  String *comma = String_create(", ");
  String *space = String_create(" ");
  String *closing = String_create("}");

  bool found = false;
  for (uword_t i = 0; i <= root->sizeMask; i++) {
    ConcurrentHashMapEntry *entry = &(root->array[i]);
    RTValue key = entry->key;
    RTValue value = entry->value;
    if (!RT_isNull(key) && !RT_isNull(value)) {
      if (found) {
        Ptr_retain(comma);
        retVal = String_concat(retVal, comma);
      }
      found = true;
      Ptr_retain(space);
      retain(key);
      retain(value);
      retVal = String_concat(retVal, toString(key));
      retVal = String_concat(retVal, space);
      retVal = String_concat(retVal, toString(value));
    }
  }
  retVal = String_concat(retVal, closing);
  Ptr_release(comma);
  Ptr_release(space);
  Ptr_release(self);
  return retVal;
}

/* outside refcount system */
void ConcurrentHashMapNode_destroy(ConcurrentHashMapNode *node) {
  for (uword_t i = 0; i <= node->sizeMask; i++) {
    ConcurrentHashMapEntry *entry = &(node->array[i]);
    RTValue key = atomic_load_explicit(&(entry->key), memory_order_relaxed);
    RTValue value = atomic_load_explicit(&(entry->value), memory_order_relaxed);
    autorelease(key);
    autorelease(value);
  }
}

/* outside refcount system */
void ConcurrentHashMap_destroy(ConcurrentHashMap *self) {
  ConcurrentHashMapNode *root =
      atomic_load_explicit(&(self->root), memory_order_relaxed);
  atomic_store(&(self->root), NULL);
  if (root) {
    Ptr_release(root);
  }
}
