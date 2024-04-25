#include "Object.h"
#include "PersistentArrayMap.h"
#include "Nil.h"
#include <stdarg.h>
#include "defines.h"

static PersistentArrayMap *EMPTY = NULL;

/* mem done */
PersistentArrayMap *PersistentArrayMap_empty() {
    if (EMPTY == NULL) EMPTY = PersistentArrayMap_create();
    retain(EMPTY);
    return EMPTY;
}

/* mem done */
PersistentArrayMap *PersistentArrayMap_create() {
    size_t size = sizeof(Object) + sizeof(PersistentArrayMap);
    Object * super = allocate(size);
    PersistentArrayMap *self = Object_data(super);
    memset(super, 0, size);
    Object_create(super, persistentArrayMapType);
    return self;
}

/* outside refcount system */
PersistentArrayMap *PersistentArrayMap_copy(PersistentArrayMap *other) {
    size_t baseSize = sizeof(PersistentArrayMap);
    size_t size = baseSize + sizeof(Object);
    Object * super = allocate(size);
    PersistentArrayMap *self = Object_data(super);
    memcpy(self, other, baseSize);
    Object_create(super, persistentArrayMapType);
    return self;
}

/* mem done */
PersistentArrayMap *PersistentArrayMap_createMany(uint64_t pairCount, ...) {
    assert(pairCount < HASHTABLE_THRESHOLD + 1 && "Maps of size > 8 not supported yet");
    va_list args;
    va_start(args, pairCount);

    PersistentArrayMap *self = PersistentArrayMap_create();

    self->count = pairCount;
    for (int i = 0; i < pairCount; i++) {
        void *k = va_arg(args,
        void *);
        void *v = va_arg(args,
        void *);
        self->keys[i] = k;
        self->values[i] = v;
    }
    va_end(args);
    return self;
}

/* outside refcount system */
BOOL PersistentArrayMap_equals(PersistentArrayMap *self, PersistentArrayMap *other) {
    if (self->count != other->count) return FALSE;
    for (int i = 0; i < self->count; i++) {
        void *key = self->keys[i];
        void *val = self->values[i];
        retain(self);
        retain(key);
        void *otherVal = PersistentArrayMap_get(other, key);
        if (!equals(otherVal, val)) return FALSE;
    }
    return TRUE;
}

/* outside refcount system */
uint64_t PersistentArrayMap_hash(PersistentArrayMap *self) {
    uint64_t h = 5381;
    for (int i = 0; i < self->count; i++) {
        void *key = self->keys[i];
        void *value = self->values[i];
        h += (hash(key) ^ hash(value));
    }
    return h;
}

/* mem done */
String *PersistentArrayMap_toString(PersistentArrayMap *self) {
    String * retVal = String_create("{");
    String * space = String_create(" ");
    String * comma = String_create(", ");
    String * closing = String_create("}");

    BOOL hasAtLeastOne = FALSE;
    for (int i = 0; i < self->count; i++) {
        void *key = self->keys[i];
        if (hasAtLeastOne) {
            retain(comma);
            retVal = String_concat(retVal, comma);
        }
        hasAtLeastOne = TRUE;
        retain(key);
        String * ks = toString(key);
        retVal = String_concat(retVal, ks);
        retain(space);
        retVal = String_concat(retVal, space);
        retain(self->values[i]);
        String * vs = toString(self->values[i]);
        retVal = String_concat(retVal, vs);
    }

    retVal = String_concat(retVal, closing);
    release(space);
    release(comma);
    release(self);
    return retVal;
}

/* outside refcount system */
void PersistentArrayMap_destroy(PersistentArrayMap *self, BOOL deallocateChildren) {
    if (deallocateChildren) {
        for (int i = 0; i < self->count; i++) {
            void *key = self->keys[i];
            void *value = self->values[i];
            release(key);
            release(value);
        }
    }
}

/* outside refcount system */
void PersistentArrayMap_retainChildren(PersistentArrayMap *self, int except) {
    for (int i = 0; i < self->count; i++) {
        void *foundKey = self->keys[i];
        if (foundKey && except != i) {
            retain(self->keys[i]);
            retain(self->values[i]);
        }
    }
}

PersistentArrayMap *PersistentArrayMap_copy(PersistentArrayMap *self) {
    size_t size = sizeof(PersistentArrayMap) + sizeof(Object);
    Object * super = allocate(size);
    PersistentArrayMap *new = Object_data(super);
    retain(Object_data(self->root));
    new->root = self->root;
    new->count = self->count;
    Object_create(super, persistentArrayMapType);
    return new;

}

/* mem done */
PersistentArrayMap *PersistentArrayMap_assoc(PersistentArrayMap *self, void *key, void *value) {
//    HERE in original code is some strange hasNull logic, skipping it for now
//    if (key == NULL) {
//
//    }
    uint64_t hash = hash(key);
    BOOL isNodeAdded = FALSE;
    HashMapNode * root = self->root || BitmapIndexedNode_empty();
    HashMapNode * newRoot = PersistentHashMapNode_assoc(root, 0, hash, key, value, &isNodeAdded);
    if (newRoot == root) {
        return self;
    }

    PersistentArrayMap *new = PersistentArrayMap_copy(self);
    new->root = newRoot;
    if (!isNodeAdded) {
        new->count += 1;
    }

    release(self->root);


}

/* mem done */
PersistentArrayMap *PersistentArrayMap_dissoc(PersistentArrayMap *self, void *key) {
    retain(self);
    int64_t found = PersistentArrayMap_indexOf(self, key);
    if (found == -1) {
        return self;
    }
    BOOL reusable = isReusable(self);
    PersistentArrayMap *retVal = reusable ? self : PersistentArrayMap_copy(self);
    void *k = retVal->keys[found];
    void *v = retVal->values[found];
    for (int64_t i = found; i < self->count - 1; i++) {
        retVal->keys[i] = self->keys[i + 1];
        retVal->values[i] = self->values[i + 1];
    }
    retVal->count--;
    if (!reusable) {
        PersistentArrayMap_retainChildren(retVal, -1);
        release(self);
    } else {
        release(k);
        release(v);
    }
    return retVal;
}

/* mem done */
int64_t PersistentArrayMap_indexOf(PersistentArrayMap *self, void *key) {
    int64_t retVal = -1;
    for (int64_t i = 0; i < self->count; i++) {
        if (equals(key, self->keys[i])) {
            retVal = i;
            break;
        }
    }
    release(self);
    release(key);
    return retVal;
}

/* mem done */
void *PersistentArrayMap_get(PersistentArrayMap *self, void *key) {
    void *retVal = NULL;
    for (int64_t i = 0; i < self->count; i++) {
        if (equals(key, self->keys[i])) {
            retVal = self->values[i];
            break;
        }
    }
    if (retVal) retain(retVal);
    release(self);
    release(key);
    return retVal ?: Nil_create();
}

/* mem done */
void *PersistentArrayMap_dynamic_get(void *self, void *key) {
    Object * type = super(self);
    if (type->type != persistentArrayMapType) {
        release(self);
        release(key);
        return Nil_create();
    }
    return PersistentArrayMap_get(self, key);
}

HashMapNode PersistentHashMap_createNode(uint32_t shift, void *key1, void *val1, uint32_t hash2, void *key2,
                                         void *val2, BOOL *isNodeAdded) {
    uint32_t hash1 = hash(key1);
    if (hash1 == hash2) {
        return HashCollisionNode_create(hash1, 2, (Object *[]) {super(key1), super(val1), super(key2), super(val2)});
    }
    HashMapNode * node = BitmapIndexedNode_create(0, NULL);


    return BitmapIndexedNode_assoc(
            BitmapIndexedNode_assoc(
                    BitmapIndexedNode_empty(),
                    shift,
                    hash1,
                    key1,
                    val1,
                    isNodeAdded
            ),
            shift,
            hash2,
            key2,
            val2,
            isNodeAdded
    );
}

HashCollisionNode *HashCollisionNode_create(uint32_t hash, uint32_t count, Object *array[]) {
    size_t size = sizeof(Object) + sizeof(HashCollisionNode) + count * sizeof(Object *) * 2;
    Object * super = allocate(size);
    HashCollisionNode *self = Object_data(super);
    self->hash = hash;
    self->count = count;
    memcpy(self->array, array, count * sizeof(Object *) * 2);
    Object_create(super, hashCollisionNodeType);
    return self;
}

HashCollisionNode *
HashCollisionNode_cloneAndSet(HashCollisionNode *nodeWithArray, uint32_t idx, void *key, void *value) {
    size_t newSize = sizeof(Object) + sizeof(HashCollisionNode) + nodeWithArray->count * 2 * sizeof(Object *);
    Object * superValue = allocate(newSize);
    HashCollisionNode *self = Object_data(superValue);
    self->hash = nodeWithArray->hash;
    self->count = nodeWithArray->count;
    memcpy(self->array, nodeWithArray->array, nodeWithArray->count * sizeof(Object *));
    for (uint32_t i = 0; i < self->count; i++) {
        if (i == idx) {
            continue;
        }
        Object_retain(self->array[i]);
    }
    self->array[idx] = super(key);
    self->array[idx + 1] = super(value);
    Object_create(superValue, hashCollisionNodeType);
    release(nodeWithArray);
    return self;
}

HashCollisionNode *
HashCollisionNode_assoc(HashCollisionNode *self, uint32_t shift, uint32_t hash, void *key, void *value,
                        BOOL *isNodeAdded) {
    if (self->hash == hash) {
        for (uint32_t i = 0; i < self->count * 2; i++) {
            if (equals(self->array[i], key)) {
                if (self->array[i + 1] == value) {
                    return self;
                }
                return HashCollisionNode_cloneAndSet(self, i, key, value);
            }
        }
        *isNodeAdded = TRUE;
        return HashCollisionNode_cloneAndSet(self, self->count * 2, key, value);
    }

    return BitmapIndexedNode_assoc(
            BitmapIndexedNode_create(BitmapIndexedNode_bitpos(self->hash, shift), (Object *[]) {NULL, self}),
            shift,
            hash,
            key,
            value,
            isNodeAdded
    );
}

BitmapIndexedNode *BitmapIndexedNode_create(uint32_t bitmap, HashMapNode *array) {
    size_t size = sizeof(Object) + sizeof(BitmapIndexedNode) + __builtin_popcount(bitmap) * 2 * sizeof(Object *);
    Object * super = allocate(size);
    BitmapIndexedNode * self = Object_data(super);
    self->bitmap = bitmap;
    if (array != NULL) {
        for (uint32_t i = 0; i < __builtin_popcount(bitmap) * 2; i++) {
            retain(array[i]);
        }
    }
    memcpy(self->array, array, sizeof(Object *) * __builtin_popcount(bitmap) * 2);
    Object_create(super, bitmapIndexedNodeType);
    return self;
}

BitmapIndexedNode *
BitmapIndexedNode_createWithInsertion(uint32_t bitmap, uint32_t arraySize, HashMapNode *oldArray, uint32_t insertIndex,
                                      void *keyToInsert, void *valueToInsert) {
    size_t newSize = sizeof(Object) + sizeof(BitmapIndexedNode) + arraySize * sizeof(Object *);
    Object * superValue = allocate(newSize);
    BitmapIndexedNode * self = Object_data(superValue);
    self->bitmap = bitmap;
    memcpy(self->array, oldArray, sizeof(Object *) * insertIndex);
    self->array[insertIndex] = super(keyToInsert);
    self->array[insertIndex + 1] = super(valueToInsert);
    memcpy(self->array + insertIndex + 2, oldArray + insertIndex, sizeof(Object *) * (size - insertIndex));
    Object_create(superValue, bitmapIndexedNodeType);
}

HashMapNode BitmapIndexedNode_assoc(BitmapIndexedNode *self, uint32_t shift, uint32_t hash, void *key, void *value,
                                    BOOL *isNodeAdded) {

    uint32_t bit = BitmapIndexedNode_bitpos(hash, shift);
    uint32_t idx = BitmapIndexedNode_index(self, bit);

    Object * keySuper = super(key);
    Object * valueSuper = super(value);

    if (self->bitmap & bit) {
        Object * keyOrNull = self->array[2 * idx];
        Object * valueOrNode = self->array[2 * idx + 1];
        // Value is a node not a value
        if (keyOrNull == NULL) {
            keyOrNull = PersistentHashMapNode_assoc(valueOrNode, shift + 5, hash, key, value, isNodeAdded);
            if (keyOrNull == valueOrNode) {
                return self;
            }
            return BitmapIndexedNode_cloneAndSet(self, 2 * idx + 1, keyOrNull);
        }
        // Key is stored in this node
        if (equals(key, keyOrNull)) {
            if (value == valueOrNode) {
                return self;
            }
            return BitmapIndexedNode_cloneAndSet(self, 2 * idx + 1, value);
        }
        *isNodeAdded = TRUE;
        return BitmapIndexedNode_cloneAndSet(
                self->array, 2 * idx, NULL, 2 * idx + 1,
                PersistentHashMap_createNode(
                        shift + 5, keyOrNull, valueOrNode, hash, key, value, isNodeAdded))
        ));
    } else {
        uint8_t bitCount = __builtin_popcount(self->bitmap);
        if (bitCount >= 16) {
            uint32_t mask = BitmapIndexedNode_mask(hash, shift);
            return ContainerNode_createFromBitmapIndexedNode(self, shift, hash, mask, key, value, isNodeAdded);
        } else {
            BitmapIndexedNode * result = BitmapIndexedNode_createWithInsertion(self->bitmap | bit, bitCount + 2,
                                                                               self->array, idx, key, value);
            *isNodeAdded = TRUE;
            release(self);
            return result;
        }
    }

}

ContainerNode *ContainerNode_createFromBitmapIndexedNode(BitmapIndexedNode *node, uint32_t shift, uint32_t insertHash,
                                                         uint32_t insertIndex, void *keyToInsert, void *valueToInsert, BOOL *isNodeAdded) {
    uint32_t count = __builtin_popcount(node->bitmap);
    size_t size = sizeof(Object) + sizeof(ContainerNode) + 32 * sizeof(Object *); // Full size node
    Object * superValue = allocate(size);
    ContainerNode *self = Object_data(superValue);
    self->count = count + 1;
    self->array[insertIndex] = BitmapIndexedNode_assoc(
            BitmapIndexedNode_empty(),
            shift + 5,
            insertHash,
            keyToInsert,
            valueToInsert,
            isNodeAdded
    );
    uint32_t idx = 0;
    for (uint32_t i = 0; i < 32; i++) {
        if ((node->bitmap >> i) & 1) {
            self->array[i] = node->array[j + 1];
            idx++;
        } else {
            self->array[i] = BitmapIndexedNode_assoc(
                    BitmapIndexedNode_empty(),
                    shift + 5,
                    hash(node->array[j]),
                    node->array[j],
                    node->array[j + 1],
                    isNodeAdded
            );
        }
        j += 2;
    }
    Object_create(super, containerNodeType);
    release(node);
    return self;
}

ContainerNode *ContainerNode_cloneAndSet(ContainerNode *nodeWithArray, uint32_t idx, HashMapNode *a) {
    size_t newSize = sizeof(Object) + sizeof(ContainerNode) + (nodeWithArray->count) * sizeof(Object *);
    Object * super = allocate(newSize);
    ContainerNode *self = Object_data(super);
    self->count = self->count;
    memcpy(self->array, nodeWithArray->array, (nodeWithArray->count) *
                                              sizeof(Object *)); // unsafe cause release on nodeWithArray may cause elements of array to be released
    for (uint32_t i = 0; i < self->count; i++) {
        retain(self->array[i]);
    }
    self->array[idx] = a;
    Object_create(super, containerNodeType);
    release(nodeWithArray);
    return self;

}

ContainerNode *ContainerNode_assoc(ContainerNode *self, uint32_t shift, uint32_t hash, void *key, void *value) {
    uint32_t idx = BitmapIndexedNode_mask(hash, shift);

    Object * node = self->array[idx];

    if (node == NULL) {
        return ContainerNode_cloneAndSet(self, idx, key, BitmapIndexedNode_assoc(
                                                 BitmapIndexedNode_empty(),
                                                 shift + 5,
                                                 hash,
                                                 key,
                                                 value,
                                                 isNodeAdded
                                         )
        );
    }

    HashMapNode * newNode = PersistentHashMapNode_assoc(node, shift + 5, hash, key, value, isNodeAdded);
    if (newNode == node) {
        return self;
    }

    return ContainerNode_cloneAndSet(self, idx, newNode);

}

BitmapIndexedNode *ContainerNode_pack(ContainerNode *node, uint32_t index) {
    uint32_t newArraySize = 2 * (node->count - 1);
    size_t newSize = sizeof(Object) + sizeof(BitmapIndexedNode) + newArraySize * sizeof(Object *);
    Object * super = allocate(newSize);
    BitmapIndexedNode * self = Object_data(super);
    self->bitmap = 0;
    uint32_t j = 0;
    // there are no keys here so we insert only values
    for (uint32_t i = 0; i < newArraySize; i++) {
        if (i == index) {
            j += 2;
            continue;
        }
        if (array[i] != NULL) {
            self->bitmap |= 1 << j;
            self->array[j] = node->array[i];
            j += 2;
        }
    }
    Object_create(super, bitmapIndexedNodeType);
    return self;
}

BitmapIndexedNode *BitmapIndexedNode_createWithout(BitmapIndexedNode *node, uint32_t bitmap, uint32_t idx) {
    uint32_t bitCount = __builtin_popcount(node->bitmap);
    size_t newSize = sizeof(Object) + sizeof(BitmapIndexedNode) + bitCount * 2 - 2 * sizeof(Object *);
    Object * super = allocate(newSize);
    BitmapIndexedNode * self = Object_data(super);
    self->bitmap = bitmap;
    memcpy(self->array, node->array, sizeof(Object *) * idx * 2);
    memcpy(self->array + idx * 2, node->array + (idx + 1) * 2, sizeof(Object *) * (self->bitmap - idx - 1) * 2);
    Object_create(super, bitmapIndexedNodeType);
    return self;
}

HashMapNode BitmapIndexedNode_dissoc(BitmapIndexedNode *self, uint32_t shift, uint32_t hash, void *key) {
    uint32_t bit = BitmapIndexedNode_bitpos(hash, shift);
    if (!(self->bitmap & bit)) {
        return self;
    }
    uint32_t idx = BitmapIndexedNode_index(self, bit);
    Object * keyOrNull = self->array[2 * idx];
    Object * valueOrNode = self->array[2 * idx + 1];
    if (keyOrNull == NULL) {
        HashMapNode * subNode = BitmapIndexedNode_dissoc(valueOrNode, shift + 5, hash, key);
        if (subNode == valueOrNode) {
            return self;
        }
        if (subNode == NULL) {
            if (self->bitmap == bit) {
                release(self);
                return NULL;
            }
            return BitmapIndexedNode_createWithout(self, self->bitmap ^ bit, idx);
        }
        return BitmapIndexedNode_cloneAndSet(self, 2 * idx + 1, subNode);
    }
    if (equals(key, keyOrNull)) {
        return BitmapIndexedNode_createWithout(self, self->bitmap ^ bit, idx);
    }
    return self;

}

HashMapNode *PersistentHashMapNode_assoc(HashMapNode *self, uint32_t shift, uint32_t hash, void *key, void *value,
                                         BOOL *isNodeAdded) {
    if (self == NULL) {
        return NULL;
    }

    switch (self->type) {
        case bitmapIndexedNode:
            return BitmapIndexedNode_assoc(self, shift, hash, key, value, isNodeAdded);
        case hashCollisionNode:
            return HashCollisionNode_assoc(self, shift, hash, key, value, isNodeAdded);
        case containerNode:
            return ContainerNode_assoc(self, shift, hash, key, value, isNodeAdded);
    }

    assert("SHOULD NOT HAPPEN" && FALSE);
    return NULL;

}


Object *BitmapIndexedNode_get(BitmapIndexedNode *self, uint32_t shift, uint32_t hash, void *key);