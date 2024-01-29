#include "Object.h"
#include "PersistentVector.h"
#include "PersistentVectorNode.h"
#include "Transient.h"
#include <stdarg.h>

PersistentVector *EMPTY_VECTOR = NULL;

/* mem done */
PersistentVector* PersistentVector_create() {  
  retain(EMPTY_VECTOR);
  return EMPTY_VECTOR;
}

/* mem done */
PersistentVector* PersistentVector_allocate(uint64_t transientID) {  
  Object *super = allocate(sizeof(PersistentVector)+ sizeof(Object)); 
  PersistentVector *self = (PersistentVector *)(super + 1);
  self->count = 0;
  self->shift = 0;
  self->transientID = transientID;
  self->root = NULL;
  self->tail = NULL;
  Object_create(super, persistentVectorType);
  return self;
}

/* mem done */
PersistentVector* PersistentVector_createMany(uint64_t objCount, ...) {
  va_list args;
  va_start(args, objCount);
  uint64_t initialCount = MIN(objCount, RRB_BRANCHING);
  PersistentVector *v = PersistentVector_allocate(PERSISTENT);
  v->tail = PersistentVectorNode_allocate(initialCount, leafNode, PERSISTENT);
  for(int i=0; i<initialCount; i++) {
    void *obj = va_arg(args, void *);
    v->tail->array[i] = super(obj);
  }
  v->count = initialCount;

  int remainingCount = objCount - initialCount;
  for(int i=0; i<remainingCount; i++) {
    v = PersistentVector_conj_internal(v, super(va_arg(args, void *)));
  }
  va_end(args);
  return v;
}

/* mem done */
void PersistentVector_initialise() {  
  EMPTY_VECTOR = PersistentVector_allocate(PERSISTENT);
  EMPTY_VECTOR->tail = PersistentVectorNode_allocate(RRB_BRANCHING, leafNode, PERSISTENT);
  EMPTY_VECTOR->tail->count = 0;
}

BOOL PersistentVector_equals(PersistentVector * restrict self, PersistentVector * restrict other) {
  if (self->count != other->count) return FALSE;
  BOOL retVal = equals(self->tail, other->tail);
  if (self->count > RRB_BRANCHING) retVal = retVal && equals(self->root, other->root);
  return retVal;
}

uint64_t PersistentVector_hash(PersistentVector * restrict self) {
  return combineHash(5381, self->root ? hash(self->root) : 5381);
}
/* mem done */
String *PersistentVector_toString(PersistentVector * restrict self) {
  String *retVal = String_create("[");
  String *space = String_create(" ");
  String *closing = String_create("]");

  if (self->count > RRB_BRANCHING) {
    retain(self->root);
    retain(space);
    retVal = String_concat(retVal, toString(self->root));
    retVal = String_concat(retVal, space);
  }
  retain(self->tail);
  
  retVal = String_concat(retVal, toString(self->tail));   
  retVal = String_concat(retVal, closing);
  release(self);
  release(space);
  return retVal;
}

/* outside refcount system */
void PersistentVector_destroy(PersistentVector * restrict self, BOOL deallocateChildren) {
  release(self->tail);
  if (self->root) release(self->root);
}

/* mem done */
PersistentVector* PersistentVector_assoc_internal(PersistentVector * restrict self, uint64_t index, void * restrict other) {
  if (index > self->count) { 
    release(self); 
    release(other); 
    return NULL; 
  }
  if (index == self->count) return PersistentVector_conj_internal(self, other);
  uint64_t tailOffset = self->count - self->tail->count;

  uint64_t transientID = self->transientID;
  BOOL reusable = isReusable(self) || transientID;
  
  if (index >= tailOffset) {
    BOOL tailReusable = isReusable(self->tail) || (transientID && (transientID == self->tail->transientID));
    PersistentVector *new;
    PersistentVectorNode *newTail;
    if (tailReusable) {
      newTail = self->tail;
      Object *old = newTail->array[index - tailOffset];
      newTail->array[index - tailOffset] = super(other);
      Object_release(old);
    } else {
      newTail = PersistentVectorNode_allocate(self->tail->count, leafNode, transientID);
      memcpy(newTail, self->tail, sizeof(PersistentVectorNode) + self->tail->count * sizeof(Object *));
      newTail->transientID = transientID;
      newTail->array[index - tailOffset] = super(other);  
      for (int i = 0; i < newTail->count; i++) if (i != index - tailOffset) Object_retain(newTail->array[i]);
      release(self->tail);
    }
    if (reusable) {
      new = self;
    } else {
      new = PersistentVector_allocate(PERSISTENT);
      memcpy(new, self, sizeof(PersistentVector));
      if (new->root) retain(new->root);
    }
    new->tail = newTail;
    return new;
  }
  
  PersistentVector *new = NULL;
  if(reusable) new = self;
  else {
    new = PersistentVector_allocate(PERSISTENT);
    memcpy(new, self, sizeof(PersistentVector));
    /* We are within tree bounds if we reached this place. Node will hold the parent node of our element */
    retain(self->tail);
    new->tail = self->tail;
  }
  
  new->root = PersistentVectorNode_replacePath(self->root, self->shift, index, super(other), reusable, transientID);
  
  if(!reusable) release(self);
  return new;
}

PersistentVector* PersistentVector_assoc(PersistentVector * restrict self, uint64_t index, void * restrict other) {
  assert_persistent(self->transientID);
  return PersistentVector_assoc_internal(self, index, other);
}

PersistentVector* PersistentVector_assoc_BANG_(PersistentVector * restrict self, uint64_t index, void * restrict other) {
  assert_transient(self->transientID);
  return PersistentVector_assoc_internal(self, index, other);
}


/* outside refcount system */
void PersistentVectorNode_print(PersistentVectorNode * restrict self) {
  if (!self) return;
  if (self->type == leafNode) { printf("*Leaf* "); return; }
  printf(" <<< NODE: %llu subnodes: ", self->count);
  for (int i=0; i<self->count; i++) {
    PersistentVectorNode_print(Object_data(self->array[i]));
  }
  printf(" >>> ");
}

/* outside refcount system */
void PersistentVector_print(PersistentVector * restrict self) {
  printf("==================\n");
  printf("Root: %llu, tail: %llu\n", self->count, self->tail->count);
  PersistentVectorNode_print(self->root);

  printf("==================\n");
}

/* mem done */
PersistentVector* PersistentVector_conj_internal(PersistentVector * restrict self, void * restrict other) {
  PersistentVector *new = NULL;
  uint64_t transientID = self->transientID;
  BOOL reusable = isReusable(self) || transientID;
  if(reusable) new = self;
  else {
    new = PersistentVector_allocate(PERSISTENT);
    /* create allocates a tail, but we do not need it since we copy tail this way or the other. */
    memcpy(new, self, sizeof(PersistentVector));
  }
  
  if (self->tail->count < RRB_BRANCHING) {
    
    new->count++;

    if(reusable) {
      /* If tail is not full, it is impossible for it to be used by any other vector */
      new->tail->array[self->tail->count] = super(other);
      new->tail->count++;    
      return new;
    }
    if(self->root) retain(self->root);
    new->tail = PersistentVectorNode_allocate(self->tail->count + 1, leafNode, transientID);
    memcpy(new->tail, self->tail, sizeof(PersistentVectorNode) + self->tail->count * sizeof(Object *));
    new->tail->transientID = transientID;
    new->tail->array[self->tail->count] = super(other);
    new->tail->count++;
    for(int i=0; i<new->tail->count - 1; i++) Object_retain(new->tail->array[i]);
    release(self);
    return new;
  }

  /* The tail was full if we reached this place */
  
  PersistentVectorNode *oldTail = self->tail;
  PersistentVectorNode *oldRoot = self->root;
  new->count++;
  new->tail = PersistentVectorNode_allocate(1, leafNode, transientID);
  new->tail->array[0] = super(other);
  
  BOOL copied;

  if(!reusable) {
    retain(oldTail);
    if(oldRoot) retain(oldRoot);
  }
  
  new->root = PersistentVectorNode_pushTail(NULL, oldRoot, oldTail, self->shift, &copied, reusable, transientID); 
  
  if(!copied && oldRoot) { 
    
    new->shift += RRB_BITS;
    
    /* int depth = 0; */
    /* PersistentVectorNode *c = new->root; */
    /* while(c->type != leafNode) { c = Object_data(c->array[0]); depth++; } */
    /* printf("--> Increasing depth from %llu to %llu, count: %llu, real depth: %d\n", new->shift-RRB_BITS, new->shift, new->count, depth); */
    /* depth = 0; */
    /* c = new->root; */
    /* while(c->type != leafNode) { c = Object_data(c->array[c->count -1]); depth++; } */
    /* printf ("!!! Right depth: %d\n", depth); */
  }
  if (!reusable) release(self);
  return new;
}

PersistentVector* PersistentVector_conj(PersistentVector * restrict self, void * restrict other) {
  assert_persistent(self->transientID);
  return PersistentVector_conj_internal(self, other);
}

PersistentVector* PersistentVector_conj_BANG_(PersistentVector * restrict self, void * restrict other) {
  assert_transient(self->transientID);
  return PersistentVector_conj_internal(self, other);
}


/* mem done */
void* PersistentVector_nth(PersistentVector * restrict self, uint64_t index) {
  /* TODO - should throw */
  if(index >= self->count) {
    release(self);
    return NULL; 
  }
  uint64_t tailOffset = self->count - self->tail->count;
  if(index >= tailOffset) {
    Object *retVal = self->tail->array[index - tailOffset];
    Object_retain(retVal);
    release(self);
    return Object_data(retVal);
  }

  PersistentVectorNode *node = self->root;
  for(uint64_t level = self->shift; level > 0; level -= RRB_BITS) {
    node = Object_data(node->array[(index >> level) & RRB_MASK]);
  }
  Object *retVal = node->array[index & RRB_MASK];
  Object_retain(retVal);
  release(self);
  return Object_data(retVal);
}

/* mem done */
void* PersistentVector_dynamic_nth(PersistentVector * restrict self, void *indexObject) {
  /* TODO: should throw */
  /* we should support big integers here as well */
  if(super(index)->type != integerType) return NULL;
  uint64_t index = ((Integer *)indexObject)->value;
  release(indexObject);
  return PersistentVector_nth(self, index);
}

PersistentVector* PersistentVector_copy_root(PersistentVector * restrict self, uint64_t transientID) {
  PersistentVector *new = PersistentVector_allocate(transientID);
  new->transientID = transientID;
  new->count = self->count;
  new->shift = self->shift;
  
  PersistentVectorNode *root = self->root;
  if (root) {
    retain(root);
    new->root = root;
  }
  
  PersistentVectorNode *tail = PersistentVectorNode_allocate(RRB_BRANCHING, leafNode, transientID);
  memcpy(tail, self->tail, sizeof(PersistentVectorNode) + self->tail->count * sizeof(Object *));
  for (int i = 0; i < tail->count; i++) Object_retain(tail->array[i]);
  new->tail = tail;
  
  if (transientID == PERSISTENT) {
    self->transientID = PERSISTENT;
  }
  
  release(self);
  return new;
}

PersistentVector* PersistentVector_transient(PersistentVector * restrict self) {
  assert_persistent(self->transientID);
  return PersistentVector_copy_root(self, getTransientID());
}

PersistentVector* PersistentVector_persistent_BANG_(PersistentVector * restrict self) {
  assert_transient(self->transientID);
  return PersistentVector_copy_root(self, PERSISTENT);
}
