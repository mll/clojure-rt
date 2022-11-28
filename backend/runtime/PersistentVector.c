#include "Object.h"
#include "PersistentVector.h"
#include "PersistentVectorNode.h"
#include "Object.h"
#include <stdarg.h>

PersistentVector *EMPTY_VECTOR = NULL;

PersistentVector* PersistentVector_create() {  
  retain(EMPTY_VECTOR);
  return EMPTY_VECTOR;
}

PersistentVector* PersistentVector_allocate() {  
  Object *super = allocate(sizeof(PersistentVector)+ sizeof(Object)); 
  PersistentVector *self = (PersistentVector *)(super + 1);
  self->count = 0;
  self->shift = 0;
  self->root = NULL;
  self->tail = NULL;
  Object_create(super, persistentVectorType);
  return self;
}

PersistentVector* PersistentVector_createMany(uint64_t objCount, ...) {
  va_list args;
  va_start(args, objCount);
  uint64_t initialCount = MIN(objCount, RRB_BRANCHING);
  PersistentVector *v = PersistentVector_allocate();
  v->tail = PersistentVectorNode_allocate(RRB_BRANCHING, leafNode);
  for(int i=0; i<initialCount; i++) {
    void *obj = va_arg(args, void *);
    v->tail->array[i] = super(obj);
  }
  v->count = initialCount;

  int remainingCount = objCount - initialCount;
  for(int i=0; i<remainingCount; i++) {
    v = PersistentVector_conj(v, super(va_arg(args, void *)));
  }
  va_end(args);
  return v;
}



void PersistentVector_initialise() {  
  EMPTY_VECTOR = PersistentVector_allocate();
  EMPTY_VECTOR->tail = PersistentVectorNode_allocate(RRB_BRANCHING, leafNode);
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

  return retVal;
}
void PersistentVector_destroy(PersistentVector * restrict self, BOOL deallocateChildren) {
  release(self->tail);
  if (self->root) release(self->root);
}

PersistentVector* PersistentVector_assoc(PersistentVector * restrict self, uint64_t index, void * restrict other) {
  if (index > self->count) { 
    release(self); 
    release(other); 
    return NULL; 
  }
  if (index == self->count) return PersistentVector_conj(self, other);
  uint64_t tailOffset = self->count - self->tail->count;

  PersistentVector *new = NULL;
  BOOL reusable = isReusable(self);
  if(reusable) new = self;
  else {
    new = PersistentVector_allocate();
    /* create allocates a tail, but we do not need it since we copy tail this way or the other. */
    memcpy(new, self, sizeof(PersistentVector));    
  }

  if (index >= tailOffset) {
    if(reusable) {
      Object *old = new->tail->array[index - tailOffset];
      new->tail->array[index - tailOffset] = super(other);  
      Object_release(old);
      return new;
    }
    
    if(self->root) retain(self->root); 
    new->tail = PersistentVectorNode_allocate(self->tail->count, leafNode);
    memcpy(new->tail, self->tail, sizeof(PersistentVectorNode) + self->tail->count * sizeof(Object *));
    new->tail->array[index - tailOffset] = super(other);  
    for(int i=0; i < new->tail->count; i++) if(i != (index - tailOffset)) Object_retain(self->tail->array[i]);
    release(self);
    return new;
  }

  /* We are within tree bounds if we reached this place. Node will hold the parent node of our element */
  if(!reusable) {
    retain(self->tail);
    new->tail = self->tail;
  }
  
  new->root = PersistentVectorNode_replacePath(self->root, self->shift, index, super(other), reusable);
  
  if(!reusable) release(self);
  return new;
}

void PersistentVectorNode_print(PersistentVectorNode * restrict self) {
  if (!self) return;
  if (self->type == leafNode) { printf("*Leaf* "); return; }
  printf(" <<< NODE: %llu subnodes: ", self->count);
  for (int i=0; i<self->count; i++) {
    PersistentVectorNode_print(Object_data(self->array[i]));
  }
  printf(" >>> ");
}

void PersistentVector_print(PersistentVector * restrict self) {
  printf("==================\n");
  printf("Root: %llu, tail: %llu\n", self->count, self->tail->count);
  PersistentVectorNode_print(self->root);

  printf("==================\n");
}


PersistentVector* PersistentVector_conj(PersistentVector * restrict self, void * restrict other) {
  PersistentVector *new = NULL;
  BOOL reusable = isReusable(self);
  if(reusable) new = self;
  else {
    new = PersistentVector_allocate();
    /* create allocates a tail, but we do not need it since we copy tail this way or the other. */
    memcpy(new, self, sizeof(PersistentVector));    
  }
  
  if (self->tail->count < RRB_BRANCHING) {
    
    new->count++;

    if(reusable) {
      new->tail->array[self->tail->count] = super(other);
      new->tail->count++;    
      return new;
    }
    if(self->root) retain(self->root); 
    new->tail = PersistentVectorNode_allocate(RRB_BRANCHING, leafNode);
    memcpy(new->tail, self->tail, sizeof(PersistentVectorNode) + self->tail->count * sizeof(Object *));
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
  new->tail = PersistentVectorNode_allocate(RRB_BRANCHING, leafNode);
  new->tail->array[0] = super(other);
  new->tail->count = 1;
  
  BOOL copied;
  if(!reusable) {
    retain(oldTail);
    if(oldRoot) retain(oldRoot);
  }

  new->root = PersistentVectorNode_pushTail(NULL, oldRoot, oldTail, self->shift, &copied, reusable); 
  
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

void* PersistentVector_dynamic_nth(PersistentVector * restrict self, void *indexObject) {
  /* TODO: should throw */
  /* we should support big integers here as well */
  if(super(index)->type != integerType) return NULL;
  uint64_t index = ((Integer *)indexObject)->value;
  release(indexObject);
  return PersistentVector_nth(self, index);
}



