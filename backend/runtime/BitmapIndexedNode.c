#include "Object.h"
#include "Nil.h"
#include <stdarg.h>
#include "defines.h"
#include "PersistentHashMap.h"

// TODO find memory corruption

static BitmapIndexedNode *EMPTY = NULL;

uint32_t BitmapIndexedNode_index(BitmapIndexedNode *self, uint32_t bit) {
    return __builtin_popcount(self->bitmap & (bit - 1));
}

uint32_t BitmapIndexedNode_mask(uint32_t hash, uint32_t shift) {
    return (hash >> shift) & 0x01f;
}

uint32_t BitmapIndexedNode_bitpos(uint32_t hash, uint32_t shift) {
    return 1 << BitmapIndexedNode_mask(hash, shift);
}

BitmapIndexedNode *BitmapIndexedNode_cloneAndSet(BitmapIndexedNode *nodeWithArray, uint32_t idx, Object *a) {
    #ifdef OBJECT_DEBUG
        assert(a->magic == 0xdeadbeef && "Memory corruption!");
    #endif
    uint32_t arraySize = __builtin_popcount(nodeWithArray->bitmap);
    Object *cloneSuper = allocate(sizeof(Object) + sizeof(BitmapIndexedNode) + sizeof(Object *) * arraySize * 2);
    BitmapIndexedNode *clone = Object_data(cloneSuper);
    memcpy(clone->array, nodeWithArray->array, sizeof(BitmapIndexedNode) * arraySize * 2);

    for (int i = 0; i < arraySize * 2; i++) {
        if (clone->array[i] == NULL || i == idx) {
            continue;
        }
        Object_retain(clone->array[i]);
    }


    clone->array[idx] = a;
    clone->bitmap = nodeWithArray->bitmap;

    Object_create(cloneSuper, bitmapIndexedNodeType);

    release(nodeWithArray);

    #ifdef OBJECT_DEBUG
        assert(cloneSuper->magic == 0xdeadbeef && "Memory corruption!");
        for (int i = 0; i < arraySize * 2; i++) {
            if (clone->array[i] != NULL) {
                assert(clone->array[i]->magic == 0xdeadbeef && "Memory corruption!");
            }
        }
    #endif

    return clone;
}

BitmapIndexedNode *
BitmapIndexedNode_cloneAndSetTwo(BitmapIndexedNode *nodeWithArray, uint32_t idx, Object *a, uint32_t idx2, Object *a2) {
    #ifdef OBJECT_DEBUG
        assert(a == NULL || a->magic == 0xdeadbeef && "Memory corruption!");
        assert(a2 == NULL || a2->magic == 0xdeadbeef && "Memory corruption!");
    #endif
    uint32_t arraySize = __builtin_popcount(nodeWithArray->bitmap);
    Object *cloneSuper = allocate(sizeof(Object) + sizeof(BitmapIndexedNode) + sizeof(Object *) * arraySize * 2);
    BitmapIndexedNode *clone = Object_data(cloneSuper);

    memcpy(clone->array, nodeWithArray->array, sizeof(BitmapIndexedNode) * arraySize * 2);

    for (int i = 0; i < arraySize * 2; i++) {
        if (clone->array[i] == NULL || i == idx || i == idx2) {
            continue;
        }
        Object_retain(clone->array[i]);
    }

    clone->array[idx] = a;
    clone->array[idx2] = a2;
    clone->bitmap = nodeWithArray->bitmap;

    Object_create(cloneSuper, bitmapIndexedNodeType);

    release(nodeWithArray);

    #ifdef OBJECT_DEBUG
        assert(cloneSuper->magic == 0xdeadbeef && "Memory corruption!");
        for (int i = 0; i < arraySize * 2; i++) {
            if (clone->array[i] != NULL) {
                assert(clone->array[i]->magic == 0xdeadbeef && "Memory corruption!");
            }
        }
    #endif

    return clone;
}

/* mem done */
BitmapIndexedNode *BitmapIndexedNode_empty() {
    if (EMPTY == NULL) EMPTY = BitmapIndexedNode_create();
    retain(EMPTY);
    return EMPTY;
}


BitmapIndexedNode *BitmapIndexedNode_create() {
    size_t size = sizeof(Object) + sizeof(BitmapIndexedNode);
    Object *super = allocate(size);
    BitmapIndexedNode *self = Object_data(super);
    self->bitmap = 0;
    Object_create(super, bitmapIndexedNodeType);
    return self;
}

BitmapIndexedNode *BitmapIndexedNode_createVa(uint32_t bitmap, uint32_t arraySize, ...) {
    size_t size = sizeof(Object) + sizeof(BitmapIndexedNode) + arraySize * sizeof(Object *) * 2;
    Object *superVal = allocate(size);
    BitmapIndexedNode *self = Object_data(superVal);
    self->bitmap = bitmap;
    va_list args;
    va_start(args, arraySize);
    for (int i = 0; i < arraySize; i++) {
        Object *key = va_arg(args, Object *);
        Object *value = va_arg(args, Object *);
        self->array[i * 2] = key;
        self->array[i * 2 + 1] = value;
    }
    va_end(args);
    Object_create(superVal, bitmapIndexedNodeType);

    #ifdef OBJECT_DEBUG
        assert(superVal->magic == 0xdeadbeef && "Memory corruption!");
        for (int i = 0; i < arraySize * 2; i++) {
            if (self->array[i] != NULL) {
                assert(self->array[i]->magic == 0xdeadbeef && "Memory corruption!");
            }
        }
    #endif

    return self;

}

BitmapIndexedNode *
BitmapIndexedNode_createWithInsertion(BitmapIndexedNode *oldSelf, uint32_t arraySize, uint32_t insertIndex,
                                      Object *keyToInsert, Object *valueToInsert, uint32_t oldBitmap) {
    size_t newSize = sizeof(Object) + sizeof(BitmapIndexedNode) + sizeof(Object *) * arraySize * 2;
    Object *superValue = allocate(newSize);
    BitmapIndexedNode *self = Object_data(superValue);
    self->bitmap = oldSelf->bitmap;
    if (arraySize > 1) {
        memcpy(self->array, oldSelf->array, sizeof(Object *) * insertIndex * 2);

        memcpy(self->array + 2 * insertIndex + 2, oldSelf->array + 2 * insertIndex,
               sizeof(Object *) * (arraySize - insertIndex - 1) * 2);
    }

    self->array[2 * insertIndex] = keyToInsert;
    self->array[2 * insertIndex + 1] = valueToInsert;

    for (uint32_t i = 0; i < arraySize * 2; i++) {
        if (self->array[i] == NULL || i == 2 * insertIndex || i == 2 * insertIndex + 1) {
            continue;
        }
        Object_retain(self->array[i]);
    }

    Object_create(superValue, bitmapIndexedNodeType);

    oldSelf->bitmap = oldBitmap;
    release(oldSelf);

    #ifdef OBJECT_DEBUG
        assert(superValue->magic == 0xdeadbeef && "Memory corruption!");
        for (int i = 0; i < arraySize * 2; i++) {
            if (self->array[i] != NULL) {
                assert(self->array[i]->magic == 0xdeadbeef && "Memory corruption!");
            }
        }
    #endif

    return self;
}

/* mem done */
Object *BitmapIndexedNode_assoc(BitmapIndexedNode *self, uint32_t shift, uint32_t hash, Object *key, Object *value,
                              BOOL *isNodeAdded) {

    #ifdef OBJECT_DEBUG
        assert(key->magic == 0xdeadbeef && "Memory corruption!");
        assert(value->magic == 0xdeadbeef && "Memory corruption!");
    #endif

    uint32_t bit = BitmapIndexedNode_bitpos(hash, shift);
    uint32_t idx = BitmapIndexedNode_index(self, bit);

    if (self->bitmap & bit) {
        Object *keyOrNull = self->array[2 * idx];
        Object *valueOrNode = self->array[2 * idx + 1];

        #ifdef OBJECT_DEBUG
            assert(keyOrNull == NULL || keyOrNull->magic == 0xdeadbeef && "Memory corruption!");
            assert(valueOrNode || valueOrNode->magic == 0xdeadbeef && "Memory corruption!");
        #endif

        // Value is a node not a value
        if (keyOrNull == NULL) {
            Object_retain(valueOrNode);
            keyOrNull =
                    PersistentHashMapNode_assoc(valueOrNode, shift + 5, hash, key, value, isNodeAdded);

            #ifdef OBJECT_DEBUG
                assert(keyOrNull->magic == 0xdeadbeef && "Memory corruption!");
            #endif

            if (keyOrNull == valueOrNode) {
                return super(self);
            }
            return super(BitmapIndexedNode_cloneAndSet(self, 2 * idx + 1, keyOrNull));
        }
        // Key is stored in this node
        if (Object_equals(key, keyOrNull)) {
            if (Object_equals(value, valueOrNode)) {
                Object_release(key);
                Object_release(value);
                return super(self);
            }
            Object_release(key);
            return super(BitmapIndexedNode_cloneAndSet(self, 2 * idx + 1, value));
        }
        *isNodeAdded = TRUE;
        return super(BitmapIndexedNode_cloneAndSetTwo(
                self, 2 * idx, NULL, 2 * idx + 1,
                PersistentHashMap_createNode(
                        shift + 5, keyOrNull, valueOrNode, hash, key, value, isNodeAdded)));
    } else {
        uint8_t bitCount = __builtin_popcount(self->bitmap);
        if (bitCount >= 16) {
            uint32_t mask = BitmapIndexedNode_mask(hash, shift);
            return ContainerNode_createFromBitmapIndexedNode(self, shift, hash, mask, key, value, isNodeAdded);
        } else {
            uint32_t oldBitmap = self->bitmap;
            self->bitmap |= bit;

            BitmapIndexedNode *result = BitmapIndexedNode_createWithInsertion(self, bitCount + 1, idx, key, value,
                                                                              oldBitmap);
            #ifdef OBJECT_DEBUG
                assert(super(result)->magic == 0xdeadbeef && "Memory corruption!");
            #endif
            *isNodeAdded = TRUE;
            return super(result);
        }
    }

}

BitmapIndexedNode *BitmapIndexedNode_createWithout(BitmapIndexedNode *node, uint32_t bitmap, uint32_t idx) {
    uint32_t bitCount = __builtin_popcount(node->bitmap);
    size_t newSize = sizeof(Object) + sizeof(BitmapIndexedNode) + bitCount * 2 - 2 * sizeof(Object *); //WTF
    Object *super = allocate(newSize);
    BitmapIndexedNode *self = Object_data(super);
    self->bitmap = bitmap;
    memcpy(self->array, node->array, sizeof(Object *) * idx * 2);
    memcpy(self->array + idx * 2, node->array + (idx + 1) * 2, sizeof(Object *) * (self->bitmap - idx - 1) * 2);
    Object_create(super, bitmapIndexedNodeType);
    return self;
}

HashMapNode *BitmapIndexedNode_dissoc(BitmapIndexedNode *self, uint32_t shift, uint32_t hash, Object *key) {
//    uint32_t bit = BitmapIndexedNode_bitpos(hash, shift);
//    if (!(self->bitmap & bit)) {
//        return self;
//    }
//    uint32_t idx = BitmapIndexedNode_index(self, bit);
//    Object *keyOrNull = self->array[2 * idx];
//    Object *valueOrNode = self->array[2 * idx + 1];
//    if (keyOrNull == NULL) {
//        HashMapNode *subNode = BitmapIndexedNode_dissoc(valueOrNode, shift + 5, hash, key);
//        if (subNode == valueOrNode) {
//            return self;
//        }
//        if (subNode == NULL) {
//            if (self->bitmap == bit) {
//                release(self);
//                return NULL;
//            }
//            return BitmapIndexedNode_createWithout(self, self->bitmap ^ bit, idx);
//        }
//        return BitmapIndexedNode_cloneAndSet(self, 2 * idx + 1, subNode);
//    }
//    if (equals(key, keyOrNull)) {
//        return BitmapIndexedNode_createWithout(self, self->bitmap ^ bit, idx);
//    }
    return self;

}

Object *BitmapIndexedNode_get(BitmapIndexedNode *self, uint32_t shift, uint32_t hash, Object *key) {
//    uint32_t bit = BitmapIndexedNode_bitpos(hash, shift);
//    if (self->bitmap & bit) {
//        uint32_t idx = BitmapIndexedNode_index(self, bit);
//        Object *keyOrNull = self->array[2 * idx];
//        Object *valueOrNode = self->array[2 * idx + 1];
//        if (keyOrNull == NULL) {
//            release(self);
//            return PersistentHashMapNode_get(valueOrNode, shift + 5, hash, key);
//        }
//        if (equals(key, keyOrNull)) {
//            release(self);
//            return valueOrNode;
//        }
//    }
//    release(self);
    return NULL;
}

BOOL BitmapIndexedNode_equals(BitmapIndexedNode *self, BitmapIndexedNode *other) {
    if (self == other) {
        return TRUE;
    }
    if (self->bitmap != other->bitmap) {
        return FALSE;
    }
    for (uint32_t i = 0; i < __builtin_popcount(self->bitmap) * 2; i++) {
        // CAN equals handle NULL?
        Object *selfElement = self->array[i];
        Object *otherElement = other->array[i];
        if ((selfElement == NULL && otherElement != NULL)
            || (selfElement != NULL && otherElement == NULL)) {
            return FALSE;
        }
        // both are NULL (we need to check only one because of the previous if statement)
        if (selfElement == NULL) {
            continue;
        }
        if (!Object_equals(selfElement, otherElement)) {
            return FALSE;
        }
    }
    return TRUE;
}

uint64_t BitmapIndexedNode_hash(BitmapIndexedNode *self) {
    uint64_t hashValue = 5381;
    for (uint32_t i = 0; i < __builtin_popcount(self->bitmap); i++) {
        uint32_t keyIndex = 2 * i;
        Object *key = self->array[keyIndex];
        Object *value = self->array[keyIndex + 1];
        if (value != NULL) {
            if (key != NULL) {
                hashValue += hash(Object_data(key)) ^ hash(Object_data(value));
            } else {
                hashValue += hash(Object_data(value));
            }
        }
    }
    return hashValue;
}

String *BitmapIndexedNode_toString(BitmapIndexedNode *self) {
//  display only when there is key and value
    if (self->bitmap == 0) {
        release(self);
        return String_create("");
    }
    String *base = String_create("");
    uint8_t idx = 0;
    for (uint32_t i = 0; i < 32; i++) {
        if (self->bitmap & (1 << i)) {
            if (self->array[2 * idx] == NULL) {
                Object_retain(self->array[2 * idx + 1]);
                String *childString = toString(Object_data(self->array[2 * idx + 1]));
                base = String_concat(base, childString);

            } else {
                Object_retain(self->array[2 * idx]);
                Object_retain(self->array[2 * idx + 1]);
                String *keyString = toString(Object_data(self->array[2 * idx]));
                String *valueString = toString(Object_data(self->array[2 * idx + 1]));

                base = String_concat(base, keyString);
                base = String_concat(base, String_create(" "));
                base = String_concat(base, valueString);
            }
            if (idx < __builtin_popcount(self->bitmap) - 1) {
                base = String_concat(base, String_create(", "));
            }
            idx += 1;
        }
    }
    release(self);
    return base;

}

void BitmapIndexedNode_destroy(BitmapIndexedNode *self, BOOL deallocateChildren) {
    if (deallocateChildren) {
        uint8_t idx = 0;
        for (uint32_t i = 0; i < __builtin_popcount(self->bitmap); i++) {
            if (self->bitmap & (1 << i)) {
                if (self->array[2 * idx] != NULL) {
                    Object_release(self->array[2 * idx]);
                }
                Object_release(self->array[2 * idx + 1]);
                idx += 1;
            }
        }
    }
}