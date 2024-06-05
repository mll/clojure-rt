#include "Object.h"
#include "ContainerNode.h"
#include "Nil.h"
#include "defines.h"
#include "PersistentHashMap.h"

Object *ContainerNode_createFromBitmapIndexedNode(BitmapIndexedNode *node, uint32_t shift, uint32_t insertHash,
                                                         uint32_t insertIndex, Object *keyToInsert, Object *valueToInsert,
                                                         BOOL *isNodeAdded) {
    uint32_t count = __builtin_popcount(node->bitmap);
    size_t size = sizeof(Object) + sizeof(ContainerNode) + 32 * sizeof(Object *); // Full size node
    Object *superValue = allocate(size);
    Object_create(superValue, containerNodeType);
    ContainerNode *self = Object_data(superValue);
    self->count = count + 1;

    #ifdef OBJECT_DEBUG
        assert(keyToInsert->magic == 0xdeadbeef && "Memory corruption!");
        assert(valueToInsert->magic == 0xdeadbeef && "Memory corruption!");
    #endif

    self->array[insertIndex] = BitmapIndexedNode_assoc(
            BitmapIndexedNode_empty(),
            shift + 5,
            insertHash,
            keyToInsert,
            valueToInsert,
            isNodeAdded
    );

    uint32_t idx = 0;
    // NAPRAWIĆ TO, COŚ TU NIE DZIAŁA!
    for (uint32_t i = 0; i < 32; i++) {
        if ((node->bitmap >> i) & 1) {
            // we have a key, value pair here
            if (node->array[2 * idx] != NULL) {
                Object_retain(node->array[2 * idx]);
                Object_retain(node->array[2 * idx + 1]);
                self->array[i] = BitmapIndexedNode_assoc(
                        BitmapIndexedNode_empty(),
                        shift + 5,
                        Object_hash(node->array[2 * idx]),
                        node->array[2 * idx],
                        node->array[2 * idx + 1],
                        isNodeAdded
                );
            } else {
                #ifdef OBJECT_DEBUG
                    assert(node->array[2 * idx + 1] != NULL && "Value is NULL!");
                #endif

                Object_retain(node->array[2 * idx + 1]);
                self->array[i] = node->array[2 * idx + 1];
            }
            idx += 1;
        } else {
            self->array[i] = super(BitmapIndexedNode_empty());
        }
    }

    release(node);

    #ifdef OBJECT_DEBUG
        assert(superValue->magic == 0xdeadbeef && "Memory corruption!");
        for (uint32_t i = 0; i < 32; i++) {
            if (self->array[i] != NULL) {
                assert(self->array[i]->magic == 0xdeadbeef && "Memory corruption!");
            }
        }
    #endif

    return superValue;
}

ContainerNode *ContainerNode_cloneAndSet(ContainerNode *nodeWithArray, uint32_t idx, Object *a) {

    #ifdef OBJECT_DEBUG
        assert(a->magic == 0xdeadbeef && "Memory corruption!");
    #endif

    size_t newSize = sizeof(Object) + sizeof(ContainerNode) + 32 * sizeof(Object *);
    Object *superValue = allocate(newSize);
    Object_create(superValue, containerNodeType);
    ContainerNode *self = Object_data(superValue);
    self->count = nodeWithArray->count;
    memcpy(self->array, nodeWithArray->array, 32 *
                                              sizeof(Object *));
    for (uint32_t i = 0; i < 32; i++) {
        if (i == idx || self->array[i] == NULL) {
            continue;
        }
        Object_retain(self->array[i]);
    }
    self->array[idx] = a;
    release(nodeWithArray);

    #ifdef OBJECT_DEBUG
        assert(superValue->magic == 0xdeadbeef && "Memory corruption!");
        for (uint32_t i = 0; i < 32; i++) {
            if (self->array[i] != NULL) {
                assert(self->array[i]->magic == 0xdeadbeef && "Memory corruption!");
            }
        }
    #endif

    return self;

}

Object *
ContainerNode_assoc(ContainerNode *self, uint32_t shift, uint32_t hash, Object *key, Object *value, BOOL *isNodeAdded) {
    uint32_t idx = BitmapIndexedNode_mask(hash, shift);

    Object *node = self->array[idx];

    #ifdef OBJECT_DEBUG
    assert(key->magic == 0xdeadbeef && "Memory corruption!");
    assert(value->magic == 0xdeadbeef && "Memory corruption!");
    assert(node == NULL || node->magic == 0xdeadbeef && "Memory corruption!");
    #endif


    if (node == NULL) {
        return super(ContainerNode_cloneAndSet(self, idx, BitmapIndexedNode_assoc(
                                                 BitmapIndexedNode_empty(),
                                                 shift + 5,
                                                 hash,
                                                 key,
                                                 value,
                                                 isNodeAdded
                                         ))
        );
    }

    Object_retain(node);
    Object *newNode = PersistentHashMapNode_assoc(node, shift + 5, hash, key, value, isNodeAdded);
    #ifdef OBJECT_DEBUG
        assert(newNode->magic == 0xdeadbeef && "Memory corruption!");
    #endif
    if (newNode == node) {
        Object_release(node);
        return super(self);
    }

    #ifdef OBJECT_DEBUG
        assert(newNode->magic == 0xdeadbeef && "Memory corruption!");
        ContainerNode *result = ContainerNode_cloneAndSet(self, idx, newNode);
        assert(super(result)->magic == 0xdeadbeef && "Memory corruption!");
        return super(result);
    #else
        return super(ContainerNode_cloneAndSet(self, idx, newNode));
    #endif

}

BitmapIndexedNode *ContainerNode_pack(ContainerNode *node, uint32_t index) {
    uint32_t newArraySize = 2 * (node->count - 1);
    size_t newSize = sizeof(Object) + sizeof(BitmapIndexedNode) + newArraySize * sizeof(Object *);
    Object *super = allocate(newSize);
    BitmapIndexedNode *self = Object_data(super);
    self->bitmap = 0;
    uint32_t j = 0;
    // there are no keys here so we insert only values
    for (uint32_t i = 0; i < newArraySize; i++) {
        if (i == index) {
            j += 2;
            continue;
        }
        if (self->array[i] != NULL) {
            self->bitmap |= 1 << j;
            self->array[j] = node->array[i];
            j += 2;
        }
    }
    Object_create(super, bitmapIndexedNodeType);
    return self;
}

BOOL ContainerNode_equals(ContainerNode *self, ContainerNode *other) {
    if (self->count != other->count) {
        return FALSE;
    }
    for (uint32_t i = 0; i < self->count; i++) {
        if (self->array[i] == NULL) {
            if (other->array[i] != NULL) {
                return FALSE;
            }
            i -= 1;
        } else {
            if (!Object_equals(self->array[i], other->array[i])) {
                return FALSE;
            }
        }
    }
    return TRUE;
}

uint64_t ContainerNode_hash(ContainerNode *self) {
    uint64_t hashValue = 0;
    for (uint32_t i = 0; i < self->count; i++) {
        if (self->array[i] != NULL) {
            hashValue += hash(Object_data(self->array[i]));
        }
    }
    return hashValue;
}

String *ContainerNode_toString(ContainerNode *self) {
    String *base = String_create("");
    uint8_t checked = 0;
    for (uint32_t i = 0; i < 32; i++) {
        if (self->array[i] != NULL) {
            Object_retain(self->array[i]);
            String *childRepr = toString(Object_data(self->array[i]));
            retain(childRepr);
            base = String_concat(base, childRepr);
            if (checked <= self->count && childRepr->count > 0) {
                base = String_concat(base, String_create(", "));
            }
            release(childRepr);
            if (childRepr->count > 0) {
                checked += 1;
            }
        }
    }
    release(self);
    return base;
}

void ContainerNode_destroy(ContainerNode *self, BOOL deallocateChildren) {
    if (deallocateChildren) {
        for (uint32_t i = 0; i < 32; i++) {
            if (self->array[i] != NULL) {
                Object_release(self->array[i]);
            }
        }
    }
}