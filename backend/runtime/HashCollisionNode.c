#include "Object.h"
#include "HashCollisionNode.h"
#include "Nil.h"
#include <stdarg.h>
#include "defines.h"


HashCollisionNode *HashCollisionNode_create(uint32_t hash, uint32_t count, ...) {
    size_t size = sizeof(Object) + sizeof(HashCollisionNode) + count * sizeof(Object *) * 2;
    Object * superVal = allocate(size);
    HashCollisionNode *self = Object_data(superVal);
    self->hash = hash;
    self->count = count;
    va_list args;
    va_start(args, count);
    for (int i = 0; i < count; i++) {
        Object *key = va_arg(args, Object *);
        Object *value = va_arg(args, Object *);
        self->array[i * 2] = key;
        self->array[i * 2 + 1] = value;
    }
    va_end(args);
    Object_create(superVal, hashCollisionNodeType);

    #ifdef OBJECT_DEBUG
        assert(superVal->magic == 0xdeadbeef && "Memory corruption!");
        for (uint32_t i = 0; i < 32; i++) {
            if (self->array[i] != NULL) {
                assert(self->array[i]->magic == 0xdeadbeef && "Memory corruption!");
            }
        }
    #endif

    return self;
}

HashCollisionNode *
HashCollisionNode_cloneAndSet(HashCollisionNode *nodeWithArray, uint32_t idx, Object *key, Object *value) {
    #ifdef OBJECT_DEBUG
        assert(key->magic == 0xdeadbeef && "Memory corruption!");
        assert(value->magic == 0xdeadbeef && "Memory corruption!");

    #endif
    size_t newSize = sizeof(Object) + sizeof(HashCollisionNode) + nodeWithArray->count * 2 * sizeof(Object *);
    Object * superValue = allocate(newSize);
    HashCollisionNode *self = Object_data(superValue);
    self->hash = nodeWithArray->hash;
    self->count = nodeWithArray->count;
    memcpy(self->array, nodeWithArray->array, nodeWithArray->count * sizeof(Object *));
    for (uint32_t i = 0; i < self->count; i++) {
        if (i == idx || i == idx + 1) {
            continue;
        }
        Object_retain(self->array[i]);
    }
    self->array[idx] = key;
    self->array[idx + 1] = value;
    Object_create(superValue, hashCollisionNodeType);
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
HashCollisionNode_assoc(HashCollisionNode *self, uint32_t shift, uint32_t hash, Object *key, Object *value,
                        BOOL *isNodeAdded) {
    #ifdef OBJECT_DEBUG
        assert(key->magic == 0xdeadbeef && "Memory corruption!");
        assert(value->magic == 0xdeadbeef && "Memory corruption!");
    #endif
    if (self->hash == hash) {
        for (uint32_t i = 0; i < self->count; i++) {
            if (Object_equals(self->array[2 * i], key)) {
                if (Object_equals(self->array[2 * i + 1], value)) {
                    return super(self);
                }
                return super(HashCollisionNode_cloneAndSet(self, 2 * i, key, value));
            }
        }
        *isNodeAdded = TRUE;
        return super(HashCollisionNode_cloneAndSet(self, self->count * 2, key, value));
    }

    return BitmapIndexedNode_assoc(
            BitmapIndexedNode_createVa(BitmapIndexedNode_bitpos(self->hash, shift), 1, NULL, super(self)),
            shift,
            hash,
            key,
            value,
            isNodeAdded
    );
}

void HashCollisionNode_destroy(HashCollisionNode *self, BOOL deallocateChildren) {
    if (deallocateChildren) {
        for (uint32_t i = 0; i < self->count * 2; i++) {
            Object_release(self->array[i]);
        }
    }
}
uint64_t HashCollisionNode_hash(HashCollisionNode *self) {
    uint64_t hashValue = 0;
    for (uint32_t i = 0; i < self->count; i++) {
        hashValue += hash(Object_data(self->array[2 * i])) ^ hash(Object_data(self->array[2 * i + 1]));
    }
    return hashValue;
}
BOOL HashCollisionNode_equals(HashCollisionNode *self, HashCollisionNode *other) {
    if (self->count != other->count) {
        return FALSE;
    }
    for (uint32_t i = 0; i < self->count * 2; i++) {
        if (!Object_equals(self->array[i], other->array[i])) {
            return FALSE;
        }
    }
    return TRUE;
}
String *HashCollisionNode_toString(HashCollisionNode *self) {
    String *base = String_create("");
    for (uint32_t i = 0; i < self->count; i++) {
        Object_retain(self->array[i * 2]);
        Object_retain(self->array[i * 2 + 1]);
        base = String_concat(base, toString(Object_data(self->array[i * 2])));
        base = String_concat(base, String_create(" "));
        base = String_concat(base, toString(Object_data(self->array[i * 2 + 1])));
        if (i != self->count - 1) {
            base = String_concat(base, String_create(", "));
        }
    }
    release(self);
    return base;
}