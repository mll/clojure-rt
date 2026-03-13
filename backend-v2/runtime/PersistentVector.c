#include "PersistentVector.h"
#include "Object.h"
#include "PersistentVectorNode.h"
#include "RTValue.h"
#include "Transient.h"
#include <stdarg.h>
#include "Exceptions.h"

PersistentVector *EMPTY_VECTOR = NULL;

/* mem done */
PersistentVector *PersistentVector_create() {
  Ptr_retain(EMPTY_VECTOR);
  return EMPTY_VECTOR;
}

/* mem done */
PersistentVector *PersistentVector_allocate(uword_t transientID) {
  PersistentVector *self =
      (PersistentVector *)allocate(sizeof(PersistentVector));
  self->count = 0;
  self->shift = 0;
  self->transientID = transientID;
  self->root = NULL;
  self->tail = NULL;
  Object_create((Object *)self, persistentVectorType);
  return self;
}

/* mem done */
PersistentVector *PersistentVector_createMany(int32_t objCount, ...) {
  va_list args;
  va_start(args, objCount);
  int32_t initialCount = MIN(objCount, RRB_BRANCHING);
  PersistentVector *v = PersistentVector_allocate(PERSISTENT);
  v->tail = PersistentVectorNode_allocate(initialCount, leafNode, PERSISTENT);
  for (int32_t i = 0; i < initialCount; i++) {
    RTValue obj = va_arg(args, RTValue);
    v->tail->array[i] = obj;
  }
  v->count = initialCount;

  if (objCount > initialCount) {
    int remainingCount = objCount - initialCount;
    for (int32_t i = 0; i < remainingCount; i++) {
      v = PersistentVector_conj_internal(v, va_arg(args, RTValue));
    }
  }
  va_end(args);
  return v;
}

/* mem done */
void PersistentVector_initialise() {
  EMPTY_VECTOR = PersistentVector_allocate(PERSISTENT);
  EMPTY_VECTOR->tail =
      PersistentVectorNode_allocate(RRB_BRANCHING, leafNode, PERSISTENT);
  EMPTY_VECTOR->tail->count = 0;
}

void PersistentVector_cleanup() {
  if (EMPTY_VECTOR) {
    Ptr_release(EMPTY_VECTOR);
    EMPTY_VECTOR = NULL;
  }
}

bool PersistentVector_equals(PersistentVector *restrict self,
                             PersistentVector *restrict other) {
  if (self->count != other->count)
    return false;
  bool retVal = Ptr_equals(self->tail, other->tail);
  if (self->count > RRB_BRANCHING)
    retVal = retVal && Ptr_equals(self->root, other->root);
  return retVal;
}

uword_t PersistentVector_hash(PersistentVector *restrict self) {
  uword_t h = combineHash(5381, self->root ? Ptr_hash(self->root) : 5381);
  return combineHash(h, Ptr_hash(self->tail));
}
/* mem done */
String *PersistentVector_toString(PersistentVector *restrict self) {
  String *retVal = String_create("[");
  String *space = String_create(" ");
  String *closing = String_create("]");

  if (self->count > RRB_BRANCHING) {
    Ptr_retain(self->root);
    Ptr_retain(space);
    retVal = String_concat(retVal, PersistentVectorNode_toString(self->root));
    retVal = String_concat(retVal, space);
  }
  Ptr_retain(self->tail);

  retVal = String_concat(retVal, PersistentVectorNode_toString(self->tail));
  retVal = String_concat(retVal, closing);
  Ptr_release(self);
  Ptr_release(space);
  return retVal;
}

/* outside refcount system */
void PersistentVector_destroy(PersistentVector *restrict self,
                              bool deallocateChildren) {
  Ptr_release(self->tail);
  if (self->root)
    Ptr_release(self->root);
}

void PersistentVector_promoteToShared(PersistentVector *self, uword_t current) {
  if (current & SHARED_BIT)
    return;

  if (self->transientID != PERSISTENT) {
    throwIllegalStateException_C("Cannot share a transient vector");
  }

  if (self->root) {
    promoteToShared(RT_boxPtr(self->root));
  }
  if (self->tail) {
    promoteToShared(RT_boxPtr(self->tail));
  }
  Object_promoteToSharedShallow((Object *)self, current);
}

/* mem done */
PersistentVector *
PersistentVector_assoc_internal(PersistentVector *restrict self, uword_t index,
                                RTValue other) {
  if (index > self->count) {
    uword_t cnt = self->count;
    Ptr_release(self);
    release(other);
    throwIndexOutOfBoundsException_C(index, cnt);
  }
  if (index == self->count)
    return PersistentVector_conj_internal(self, other);
  uword_t tailOffset = self->count - self->tail->count;

  uword_t transientID = self->transientID;
  bool reusable = Ptr_isReusable(self) || transientID;
  if (index >= tailOffset) {
    bool tailReusable =
        (reusable && Ptr_isReusable(self->tail)) ||
        (transientID && (transientID == self->tail->transientID));
    PersistentVector *new;
    PersistentVectorNode *newTail;
    if (tailReusable) {
      newTail = self->tail;
      RTValue old = newTail->array[index - tailOffset];
      newTail->array[index - tailOffset] = other;
      release(old);
    } else {
      newTail = PersistentVectorNode_allocate(self->tail->count, leafNode,
                                              transientID);
      memcpy(((Object *)newTail) + 1, ((Object *)self->tail) + 1,
             sizeof(PersistentVectorNode) - sizeof(Object) +
                 self->tail->count * sizeof(RTValue));
      newTail->transientID = transientID;
      newTail->array[index - tailOffset] = other;
      for (uword_t i = 0; i < newTail->count; i++)
        if (i != index - tailOffset)
          retain(newTail->array[i]);
    }
    if (reusable) {
      new = self;
    } else {
      new = PersistentVector_allocate(PERSISTENT);
      memcpy(((Object *)new) + 1, ((Object *)self) + 1,
             sizeof(PersistentVector) - sizeof(Object));
      if (new->root)
        Ptr_retain(new->root);
    }
    new->tail = newTail;
    if (!reusable)
      Ptr_release(self);
    return new;
  }

  PersistentVector *new = NULL;
  if (reusable)
    new = self;
  else {
    new = PersistentVector_allocate(PERSISTENT);
    memcpy(((Object *)new) + 1, ((Object *)self) + 1,
           sizeof(PersistentVector) - sizeof(Object));

    /* We are within tree bounds if we reached this place. Node will hold the
     * parent node of our element */
    Ptr_retain(self->tail);
    new->tail = self->tail;
  }
  new->root = PersistentVectorNode_replacePath(self->root, self->shift, index,
                                               other, reusable, transientID);

  if (!reusable)
    Ptr_release(self);
  return new;
}

PersistentVector *PersistentVector_assoc(PersistentVector *restrict self,
                                         uword_t index, RTValue other) {
  assert_persistent(self->transientID);
  return PersistentVector_assoc_internal(self, index, other);
}

PersistentVector *PersistentVector_assoc_BANG_(PersistentVector *restrict self,
                                               uword_t index, RTValue other) {
  assert_transient(self->transientID);
  return PersistentVector_assoc_internal(self, index, other);
}

/* outside refcount system */
void PersistentVectorNode_print(PersistentVectorNode *restrict self) {
  if (!self)
    return;
  if (self->type == leafNode) {
    printf("*Leaf* ");
    return;
  }
  printf(" <<< NODE: %lu subnodes: ", self->count);
  for (uword_t i = 0; i < self->count; i++) {
    PersistentVectorNode_print(
        (PersistentVectorNode *)RT_unboxPtr(self->array[i]));
  }
  printf(" >>> ");
}

/* outside refcount system */
void PersistentVector_print(PersistentVector *restrict self) {
  printf("==================\n");
  printf("Root: %lu, tail: %lu\n", self->count, self->tail->count);
  PersistentVectorNode_print(self->root);

  printf("==================\n");
}

/* mem done */
PersistentVector *
PersistentVector_conj_internal(PersistentVector *restrict self, RTValue other) {
  PersistentVector *new = NULL;
  uword_t transientID = self->transientID;
  bool reusable = Ptr_isReusable(self) || transientID;
  if (reusable)
    new = self;
  else {
    new = PersistentVector_allocate(PERSISTENT);
    /* create allocates a tail, but we do not need it since we copy tail this
     * way or the other. */
    memcpy(((Object *)new) + 1, ((Object *)self) + 1,
           sizeof(PersistentVector) - sizeof(Object));
  }
  new->count++;

  if (self->tail->count < RRB_BRANCHING) {
    if (reusable) {
      /* If tail is not full, it is impossible for it to be used by any other
       * vector */
      new->tail->array[self->tail->count] = other;
      new->tail->count++;
      return new;
    }
    if (self->root)
      Ptr_retain(self->root);
    new->tail = PersistentVectorNode_allocate(self->tail->count + 1, leafNode,
                                              transientID);

    memcpy(((Object *)new->tail) + 1, ((Object *)self->tail) + 1,
           sizeof(PersistentVectorNode) - sizeof(Object) +
               self->tail->count * sizeof(RTValue));
    new->tail->transientID = transientID;
    new->tail->array[self->tail->count] = other;
    new->tail->count++;
    for (uword_t i = 0; i < new->tail->count - 1; i++)
      retain(new->tail->array[i]);
    Ptr_release(self);
    return new;
  }

  /* The tail was full if we reached this place */

  PersistentVectorNode *oldTail = self->tail;
  PersistentVectorNode *oldRoot = self->root;
  new->tail = PersistentVectorNode_allocate(1, leafNode, transientID);
  new->tail->array[0] = other;

  bool copied;

  if (!reusable) {
    Ptr_retain(oldTail);
    if (oldRoot)
      Ptr_retain(oldRoot);
  }

  new->root = PersistentVectorNode_pushTail(NULL, oldRoot, oldTail, self->shift,
                                            &copied, reusable, transientID);

  if (!copied && oldRoot) {

    new->shift += RRB_BITS;

    /* int depth = 0; */
    /* PersistentVectorNode *c = new->root; */
    /* while(c->type != leafNode) { c = c->array[0]; depth++; } */
    /* printf("--> Increasing depth from %llu to %llu, count: %llu, real depth:
     * %d\n", new->shift-RRB_BITS, new->shift, new->count, depth); */
    /* depth = 0; */
    /* c = new->root; */
    /* while(c->type != leafNode) { c = c->array[c->count -1]; depth++; } */
    /* printf ("!!! Right depth: %d\n", depth); */
  }
  if (!reusable)
    Ptr_release(self);
  return new;
}

PersistentVector *PersistentVector_conj(PersistentVector *restrict self,
                                        RTValue other) {
  assert_persistent(self->transientID);
  return PersistentVector_conj_internal(self, other);
}

PersistentVector *PersistentVector_conj_BANG_(PersistentVector *restrict self,
                                              RTValue other) {
  assert_transient(self->transientID);
  return PersistentVector_conj_internal(self, other);
}

/* outside memory system */
PersistentVectorNode *PersistentVector_nthBlock(PersistentVector *self,
                                                uword_t index) {
  if (index >= self->count) {
    return NULL;
  }

  uword_t tailOffset = self->count - self->tail->count;
  if (index >= tailOffset) {
    return self->tail;
  }

  PersistentVectorNode *node = self->root;
  for (uword_t level = self->shift; level > 0; level -= RRB_BITS) {
    node = (PersistentVectorNode *)RT_unboxPtr(
        node->array[(index >> level) & RRB_MASK]);
  }
  return node;
}

/* mem done */
RTValue PersistentVector_nth(PersistentVector *restrict self, uword_t index) {
  if (index >= self->count) {
    uword_t cnt = self->count;
    Ptr_release(self);
    throwIndexOutOfBoundsException_C(index, cnt);
  }
  uword_t tailOffset = self->count - self->tail->count;
  if (index >= tailOffset) {
    RTValue retVal = self->tail->array[index - tailOffset];
    retain(retVal);
    Ptr_release(self);
    return retVal;
  }

  PersistentVectorNode *node = self->root;
  for (uword_t level = self->shift; level > 0; level -= RRB_BITS) {
    node = (PersistentVectorNode *)RT_unboxPtr(
        node->array[(index >> level) & RRB_MASK]);
  }
  RTValue retVal = node->array[index & RRB_MASK];
  retain(retVal);
  Ptr_release(self);
  return retVal;
}

/* mem done */
RTValue PersistentVector_dynamic_nth(PersistentVector *restrict self,
                                     RTValue indexObject) {
  if (!RT_isInt32(indexObject)) {
    Ptr_release(self);
    release(indexObject);
    throwIllegalArgumentException_C("Index must be an integer");
  }
  uword_t index = RT_unboxInt32(indexObject);
  return PersistentVector_nth(self, index);
}

PersistentVector *PersistentVector_copy_root(PersistentVector *restrict self,
                                             uword_t transientID) {
  PersistentVector *new = PersistentVector_allocate(transientID);
  new->transientID = transientID;
  new->count = self->count;
  new->shift = self->shift;

  PersistentVectorNode *root = self->root;
  if (root) {
    Ptr_retain(root);
    new->root = root;
  }

  PersistentVectorNode *tail =
      PersistentVectorNode_allocate(RRB_BRANCHING, leafNode, transientID);

  memcpy(((Object *)tail) + 1, ((Object *)self->tail) + 1,
         sizeof(PersistentVectorNode) - sizeof(Object) +
             self->tail->count * sizeof(RTValue));
  for (uword_t i = 0; i < tail->count; i++)
    retain(tail->array[i]);
  new->tail = tail;

  if (transientID == PERSISTENT) {
    self->transientID = PERSISTENT;
  }

  Ptr_release(self);
  return new;
}

PersistentVector *PersistentVector_transient(PersistentVector *restrict self) {
  assert_persistent(self->transientID);
  return PersistentVector_copy_root(self, getTransientID());
}

PersistentVector *
PersistentVector_persistent_BANG_(PersistentVector *restrict self) {
  assert_transient(self->transientID);
  return PersistentVector_copy_root(self, PERSISTENT);
}

PersistentVector *
PersistentVector_pop_internal(PersistentVector *restrict self) {
  if (!self->count) {
    Ptr_release(self);
    throwIllegalStateException_C("Can't pop empty vector");
  }

  PersistentVector *new;
  uword_t transientID = self->transientID;
  bool reusable = Ptr_isReusable(self) || transientID;
  if (reusable) {
    new = self;
  } else {
    new = PersistentVector_allocate(transientID);
    memcpy(((Object *)new) + 1, ((Object *)self) + 1,
           sizeof(PersistentVector) - sizeof(Object));
  }
  --new->count;

  PersistentVectorNode *newTail = NULL;
  if (self->tail->count > 1) {
    // Root remains unmodified
    if (!reusable && self->root)
      Ptr_retain(self->root);
    bool tailReusable =
        (reusable && Ptr_isReusable(self->tail)) ||
        (transientID && (transientID == self->tail->transientID));
    uword_t newCount = self->tail->count - 1;
    if (tailReusable) {
      newTail = self->tail;
      release(newTail->array[newCount]);
      if (!reusable)
        Ptr_retain(newTail);
    } else {
      newTail = PersistentVectorNode_allocate(newCount, leafNode, transientID);
      memcpy(((Object *)newTail) + 1, ((Object *)self->tail) + 1,
             sizeof(PersistentVectorNode) - sizeof(Object) +
                 newCount * sizeof(RTValue));
      for (uword_t i = 0; i < newCount; i++)
        retain(newTail->array[i]);
      if (reusable)
        Ptr_release(self->tail);
    }
    --newTail->count;
    newTail->transientID = transientID;
    new->tail = newTail;
    if (!reusable)
      Ptr_release(self);
    return new;
  }
  // self->tail->count == 1: pop furthest leaf out of root tree
  PersistentVectorNode *oldTail = self->tail;
  PersistentVectorNode *oldRoot = self->root;
  PersistentVectorNode *newRoot = NULL;
  if (oldRoot) {
    newRoot =
        PersistentVectorNode_popTail(oldRoot, &newTail, reusable, transientID);
  } else {
    newTail = PersistentVectorNode_allocate(0, leafNode, transientID);
  }
  new->tail = newTail;

  bool reduceNodeTree =
      newRoot && new->shift > 0 &&
      newRoot->count == 1; // Top node of Node tree has only one child
  if (reduceNodeTree) {
    PersistentVectorNode *newNewRoot =
        (PersistentVectorNode *)RT_unboxPtr(newRoot->array[0]);
    Ptr_retain(newNewRoot);
    if (newRoot != oldRoot)
      Ptr_release(newRoot);
    new->shift -= RRB_BITS;
    new->root = newNewRoot;
  } else {
    new->root = newRoot;
  }

  if (reusable) {
    Ptr_release(oldTail);
    if (oldRoot && (reduceNodeTree || (newRoot != oldRoot)))
      Ptr_release(oldRoot);
  } else {
    Ptr_release(self);
  }
  return new;
}

PersistentVector *PersistentVector_pop(PersistentVector *restrict self) {
  assert_persistent(self->transientID);
  return PersistentVector_pop_internal(self);
}

PersistentVector *PersistentVector_pop_BANG_(PersistentVector *restrict self) {
  assert_transient(self->transientID);
  return PersistentVector_pop_internal(self);
}

uword_t PersistentVector_count(PersistentVector *restrict self) {
  uword_t retVal = self->count;
  Ptr_release(self);
  return retVal;
}

bool PersistentVector_contains(PersistentVector *restrict self, RTValue other) {
  if (self->root) {
    Ptr_retain(self->root);
    retain(other);
    bool retVal = PersistentVectorNode_contains(self->root, other);
    if (retVal) {
      Ptr_release(self);
      release(other);
      return retVal;
    }
  }
  Ptr_retain(self->tail);
  bool retVal = PersistentVectorNode_contains(self->tail, other);
  Ptr_release(self);
  return retVal;
}
