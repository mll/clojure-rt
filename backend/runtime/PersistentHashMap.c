#include "Object.h"
#include "PersistentHashMap.h"
#include "Nil.h"
#include <stdarg.h>
#include "defines.h"


static PersistentHashMap *EMPTY = NULL;

/* mem done */
PersistentHashMap *PersistentHashMap_empty() {
    if (EMPTY == NULL) EMPTY = PersistentHashMap_create();
    retain(EMPTY);
    return EMPTY;
}

/* mem done */
PersistentHashMap *PersistentHashMap_create() {
    size_t size = sizeof(Object) + sizeof(PersistentHashMap);
    Object *superVal = allocate(size);
    PersistentHashMap *self = Object_data(superVal);
    memset(superVal, 0, size);
    Object_create(superVal, persistentHashMapType);
    return self;
}

/* mem done */
//PersistentHashMap *PersistentHashMap_createMany(uint64_t pairCount, ...) {
//    assert(pairCount < HASHTABLE_THRESHOLD + 1 && "Maps of size > 8 not supported yet");
//    va_list args;
//    va_start(args, pairCount);
//
//    PersistentHashMap *self = PersistentHashMap_create();
//
//    self->count = pairCount;
//    for (int i = 0; i < pairCount; i++) {
//        void *k = va_arg(args,
//        void *);
//        void *v = va_arg(args,
//        void *);
//        self->keys[i] = k;
//        self->values[i] = v;
//    }
//    va_end(args);
//    return self;
//}

/* outside refcount system */
BOOL PersistentHashMap_equals(PersistentHashMap *const self, PersistentHashMap *const other) {
    if (self->count != other->count) return FALSE;

    if (self->root == NULL && other->root == NULL) {
        return TRUE;
    }

    if (self->nilValue != other->nilValue) {
        return FALSE;
    }

    if (self->root == NULL || other->root == NULL) {
        return FALSE;
    }

    return equals(self->root, other->root);
}

/* outside refcount system */
uint64_t PersistentHashMap_hash(PersistentHashMap *const self) {
    uint64_t h = 5381 + hash(Object_data(self->root));

    if (self->nilValue != NULL) {
        Nil *nil = Nil_create();
        h += hash(Object_data(self->nilValue));
        h += hash(nil);
        release(nil);
    }

    return h;
}

/* mem done */
String *PersistentHashMap_toString(PersistentHashMap *const self) {
    String *retVal = String_create("{");
    String *closing = String_create("}");

    if (self->nilValue != NULL) {
        Object_retain(self->nilValue);
        String *nilStr = toString(Object_data(self->nilValue));
        retVal = String_concat(retVal, String_create("nil "));
        retVal = String_concat(retVal, nilStr);
        retVal = String_concat(retVal, String_create(", "));
    }

    if (self->root == NULL) {
        release(self);
        return String_concat(retVal, closing);
    }

    Object_retain(self->root);
    String *rootStr = toString(Object_data(self->root));

    retVal = String_concat(retVal, rootStr);
    release(self);
    return String_concat(retVal, closing);
}

/* outside refcount system */
void PersistentHashMap_destroy(PersistentHashMap *self, BOOL deallocateChildren) {
    if (self->root != NULL) {
        Object_destroy(self->root, deallocateChildren);
    }
    if (self->nilValue != NULL) {
        Object_destroy(self->nilValue, deallocateChildren);
    }
}

/* outside refcount system */
//void PersistentHashMap_retainChildren(PersistentHashMap *self, int except) {
//    for (int i = 0; i < self->count; i++) {
//        void *foundKey = self->keys[i];
//        if (foundKey && except != i) {
//            retain(self->keys[i]);
//            retain(self->values[i]);
//        }
//    }
//}

PersistentHashMap *PersistentHashMap_copy(PersistentHashMap *self) {
    size_t size = sizeof(Object) + sizeof(PersistentHashMap);
    Object *superVal = allocate(size);
    PersistentHashMap *new = Object_data(superVal);
    if (self->root != NULL) {
        Object_retain(self->root);
    }

    if (self->nilValue != NULL) {
        Object_retain(self->nilValue);
        new->nilValue = self->nilValue;
    } else {
        new->nilValue = NULL;
    }

    new->root = self->root;
    new->count = self->count;

    Object_create(superVal, persistentHashMapType);
    release(self);
    return new;

}

/* mem done */
PersistentHashMap *PersistentHashMap_assoc(PersistentHashMap *const self, Object *const key, Object *const value) {
    if (equals(Object_data(key), Nil_getUnique())) {
        if (equals(Object_data(value), Object_data(self->nilValue))) {
            Object_release(key);
            Object_release(value);
            return self;
        }
        PersistentHashMap *new = PersistentHashMap_copy(self);
        new->nilValue = value;
        new->count += 1;
        Object_release(key);
        return new;
    }
    uint64_t hashValue = Object_hash(key);
    BOOL isNodeAdded = FALSE;
    Object *root = self->root;

    if (root == NULL) {
        root = super(BitmapIndexedNode_empty());
    }

    Object_retain(root);
    Object *newRoot = PersistentHashMapNode_assoc(
            root,
            0,
            hashValue,
            key,
            value,
            &isNodeAdded
    );
    if (newRoot == root) {
        Object_release(root);
        return self;
    }

    PersistentHashMap *new = PersistentHashMap_copy(self);
    new->root = newRoot;
    if (isNodeAdded) {
        new->count += 1;
    }
    return new;
}

Object *PersistentHashMapNode_assoc(Object *const self, uint32_t shift, uint32_t hash, Object *const key, Object *const value,
                                  BOOL *isNodeAdded) {
//  NULL should not be a case here for self
#ifdef OBJECT_DEBUG
    assert(self->magic == 0xdeadbeef && "Memory corruption!");
    assert(key->magic == 0xdeadbeef && "Memory corruption!");
    assert(value->magic == 0xdeadbeef && "Memory corruption!");
#endif

    switch (self->type) {
        case bitmapIndexedNodeType:
            return BitmapIndexedNode_assoc(Object_data(self), shift, hash, key, value, isNodeAdded);
        case containerNodeType:
            return ContainerNode_assoc(Object_data(self), shift, hash, key, value, isNodeAdded);
        case hashCollisionNodeType:
            return HashCollisionNode_assoc(Object_data(self), shift, hash, key, value, isNodeAdded);
        default:
            assert(FALSE && "Should not happen for PersistentHashMapNode_assoc");
    }
}

PersistentHashMap *PersistentHashMap_dissoc(PersistentHashMap *self, void *key) {
    return NULL;
}

int64_t PersistentHashMap_indexOf(PersistentHashMap *self, void *key) {
    return -1;
}

void *PersistentHashMap_get(PersistentHashMap *self, void *key) {
    return NULL;
}

void *PersistentHashMap_dynamic_get(void *self, void *key) {
    return NULL;
}

Object *PersistentHashMap_createNode(uint32_t shift, Object *const key1, Object *const val1, uint32_t hash2, Object *const key2,
                                   Object *const val2, BOOL *isNodeAdded) {
    uint32_t hash1 = Object_hash(key1);
    if (hash1 == hash2) {
        return super(HashCollisionNode_create(hash1, 2, key1, val1, key2, val2));
    }


    BitmapIndexedNode *node = BitmapIndexedNode_empty();

    node = Object_data(BitmapIndexedNode_assoc(node, shift, hash1, key1, val1, isNodeAdded));

    return BitmapIndexedNode_assoc(node, shift, hash2, key2, val2, isNodeAdded);
}

