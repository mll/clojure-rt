#include "Object.h"
#include "PersistentVector.h"
#include "PersistentVectorNode.h"
#include "Object.h"
#include "sds/sds.h"

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

void PersistentVector_initialise() {  
  EMPTY_VECTOR = PersistentVector_allocate();
  EMPTY_VECTOR->tail = PersistentVectorNode_allocate(0, leafNode);
}

BOOL PersistentVector_equals(PersistentVector * restrict self, PersistentVector * restrict other) {
  if (self->count != other->count) return FALSE;
  BOOL retVal = equals(self->tail, other->tail);
  if (self->count > RRB_BRANCHING) retVal = retVal && equals(self->root, other->root);
  return retVal;
}

uint64_t PersistentVector_hash(PersistentVector * restrict self) {
  return combineHash(5381, self->root ? hash(self->root) : 1);
}

String *PersistentVector_toString(PersistentVector * restrict self) {
  sds retVal = sdsnew("[");

  if (self->count > RRB_BRANCHING) {
    String *s = toString(self->root);
    retVal = sdscatsds(retVal, s->value);
    retVal = sdscat(retVal, " ");
    release(s);
  }
  
  String *s = toString(self->tail);
  retVal = sdscatsds(retVal, s->value);
  retVal = sdscat(retVal, " ");
  release(s);
  
  retVal = sdscat(retVal, "]");
  return String_create(retVal);
}
void PersistentVector_destroy(PersistentVector * restrict self, BOOL deallocateChildren) {
  release(self->tail);
  if (self->root) release(self->root);
}

PersistentVector* PersistentVector_assoc(PersistentVector * restrict self, uint64_t index, Object * restrict other) {
  if (index > self->count) return NULL;
  if (index == self->count) return PersistentVector_conj(self, other);
  uint64_t tailOffset = self->count - self->tail->count;
  PersistentVector *new = PersistentVector_allocate();
  memcpy(new, self, sizeof(PersistentVector));
  
  if (index >= tailOffset) {
    if(self->root) retain(self->root); 
    new->tail = PersistentVectorNode_allocate(self->tail->count, leafNode);
    memcpy(new->tail, self->tail, sizeof(PersistentVectorNode) + self->tail->count * sizeof(Object *));
    new->tail->array[index - tailOffset] = other;  
    for(int i=0; i < new->tail->count; i++) Object_retain(self->tail->array[i]);
    return new;
  }

  /* We are within tree bounds if we reached this place. Node will hold the parent node of our element */
  retain(self->tail);
  new->tail = self->tail;
  new->root = PersistentVectorNode_replacePath(self->root, self->shift, index, other);  
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


PersistentVector* PersistentVector_conj(PersistentVector * restrict self, Object * restrict other) {
  PersistentVector *new = PersistentVector_allocate();
  /* create allocates a tail, but we do not need it since we copy tail this way or the other. */
  memcpy(new, self, sizeof(PersistentVector));
  new->count++;

  if (self->tail->count < RRB_BRANCHING) {
    if(self->root) retain(self->root); 
    new->tail = PersistentVectorNode_allocate(self->tail->count + 1, leafNode);
    memcpy(new->tail, self->tail, sizeof(PersistentVectorNode) + self->tail->count * sizeof(Object *));
    new->tail->array[self->tail->count] = other;
    new->tail->count++;
    for(int i=0; i<new->tail->count; i++) Object_retain(new->tail->array[i]);
    return new;
  }
  /* The tail was full if we reached this place */
  
  new->tail = PersistentVectorNode_allocate(1, leafNode);
  new->tail->array[0] = other;
  Object_retain(other);
  
  BOOL copied;
  new->root = PersistentVectorNode_pushTail(NULL, self->root,  self->tail, self->shift, &copied); 
  if(!copied && self->root) { 
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
  return new;
}

Object* PersistentVector_nth(PersistentVector * restrict self, uint64_t index) {
  if(index >= self->count) return NULL; 
  uint64_t tailOffset = self->count - self->tail->count;
  if(index >= tailOffset) return self->tail->array[index - tailOffset];

  PersistentVectorNode *node = self->root;
  for(uint64_t level = self->shift; level > 0; level -= RRB_BITS) {
    node = Object_data(node->array[(index >> level) & RRB_MASK]);
  }
  return node->array[index & RRB_MASK];
}



