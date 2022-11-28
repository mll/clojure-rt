#include "Object.h"
#include "PersistentVector.h"
#include "PersistentVectorNode.h"
#include "Object.h"
#include <stdarg.h>

PersistentVector *EMPTY_VECTOR = NULL;

/* mem done */
PersistentVector* PersistentVector_create() {  
  retain(EMPTY_VECTOR);
  return EMPTY_VECTOR;
}

/* mem done */
PersistentVector* PersistentVector_allocate() {  
  Object *super = allocate(sizeof(PersistentVector)+ sizeof(Object)); 
  PersistentVector *self = (PersistentVector *)(super + 1);
  self->count = 0;
  self->shift = -RRB_BITS;
  self->root = NULL;
  self->tail = NULL;
  Object_create(super, persistentVectorType);
  return self;
}

/* mem done */
PersistentVector* PersistentVector_createMany(uint64_t objCount, ...) {
  va_list args;
  va_start(args, objCount);
  uint64_t initialCount = MIN(objCount, 32);
  PersistentVector *v = PersistentVector_allocate();
  v->tail = PersistentVectorNode_allocate(initialCount, leafNode);
  for(int i=0; i<initialCount; i++) {
    void *obj = va_arg(args, void *);
    v->tail->array[i] = obj;
  }
  v->count = initialCount;

  int remainingCount = objCount - initialCount;
  for(int i=0; i<remainingCount; i++) {
    PersistentVector *s = PersistentVector_conj(v, va_arg(args, void *));
    v = s;
  }
  va_end(args);
  return v;
}


/* mem done */
void PersistentVector_initialise() {  
  EMPTY_VECTOR = PersistentVector_allocate();
  EMPTY_VECTOR->tail = PersistentVectorNode_allocate(0, leafNode);
}

/* outside refcount system */
BOOL PersistentVector_equals(PersistentVector * restrict self, PersistentVector * restrict other) {
  if (self->count != other->count) return FALSE;
  BOOL retVal = equals(self->tail, other->tail);
  if (self->count > RRB_BRANCHING) retVal = retVal && equals(self->root, other->root);
  return retVal;
}

/* outside refcount system */
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
    String *s = toString(self->root);
    retVal = String_concat(retVal, s);
    retain(space);
    retVal = String_concat(retVal, space);
  }
  
  retain(self->tail);
  String *s = toString(self->tail);
  retVal = String_concat(retVal, s);
  release(s);
  
  retVal = String_concat(retVal, closing);
  release(space);
  release(self);
  return retVal;
}

/* outside refcount system */
void PersistentVector_destroy(PersistentVector * restrict self, BOOL deallocateChildren) {
  release(self->tail);
  if (self->root) release(self->root);
}

/* mem done */
PersistentVector* PersistentVector_assoc(PersistentVector * restrict self, uint64_t index, void * restrict other) {
  if (index > self->count) {
    release(other);
    release(self);
    return NULL;
  }
  if (index == self->count) return PersistentVector_conj(self, other);
  uint64_t tailOffset = self->count - self->tail->count;
  BOOL reusable = isReusable(self);

  PersistentVector *new = NULL; 
  if(reusable) {
    /* Reusing old node */
    new = self;
    if (index >= tailOffset) {
      /* We exploit the fact that tail always belongs exclusively to one vector. */
      release(new->tail->array[index - tailOffset]);
      new->tail->array[index - tailOffset] = other;  
      return new;
    }

    /* We are within tree bounds if we reached this place. Node will hold the parent node of our element */
    new->root = PersistentVectorNode_replacePath(new->root, new->shift, index, other);  
    return new;
  } else {
    new = PersistentVector_allocate();
    memcpy(new, self, sizeof(PersistentVector));

    if (index >= tailOffset) {
      if(self->root) retain(self->root); 
      new->tail = PersistentVectorNode_allocate(self->tail->count, leafNode);
      memcpy(new->tail, self->tail, sizeof(PersistentVectorNode) + self->tail->count * sizeof(Object *));
      new->tail->array[index - tailOffset] = other;  
      for(int i=0; i < new->tail->count; i++) if(i!=index - tailOffset) retain(new->tail->array[i]);
      release(self);
      return new;
    }

    /* We are within tree bounds if we reached this place. Node will hold the parent node of our element */
    retain(self->tail);
    new->tail = self->tail;
    retain(self->root);
    new->root = PersistentVectorNode_replacePath(self->root, self->shift, index, other);  
    return new;
  }
}

/* outside refcount system */
void PersistentVectorNode_print(PersistentVectorNode * restrict self) {
  if (!self) return;
  if (self->type == leafNode) { printf("*Leaf* [%p] ", self); return; }
  printf(" <<< NODE: %llu [%p] subnodes: ", self->count, self);
  for (int i=0; i<self->count; i++) {
    PersistentVectorNode_print(self->array[i]);
  }
  printf(" >>> ");
}

/* outside refcount system */
void PersistentVector_print(PersistentVector * restrict self) {
  printf("==================\n");
  printf("Root+tail: %llu, tail: %llu\n", self->count, self->tail->count);
  PersistentVectorNode_print(self->root);

  printf("==================\n");
}

/* mem done */
PersistentVector* PersistentVector_conj(PersistentVector * restrict self, void * restrict other) {
  PersistentVector *new = NULL;
  
  BOOL reusable = isReusable(self);
  if(reusable) {
    new = self;
    new->count++;      
    if (self->tail->count < RRB_BRANCHING) {      
      new->tail->array[self->tail->count] = other;
      new->tail->count++;
      return new;
    }

    /* The tail was full if we reached this place */
     
    BOOL copied;
    new->root = PersistentVectorNode_pushTail(NULL, new->root,  new->tail, new->shift, &copied); 

    if(!copied && new->root) { 
      new->shift += RRB_BITS;
      
      int depth = 0;
      PersistentVectorNode *c = new->root;
      while(c->type != leafNode) { c = c->array[0]; depth++; }
      printf("--> Reuse! Increasing depth from %llu to %llu, count: %llu, real depth: %d\n", new->shift-RRB_BITS, new->shift, new->count, depth);
      depth = 0;
      c = new->root;
      while(c->type != leafNode) { c = c->array[c->count -1]; depth++; }
      printf ("!!! Right depth: %d\n", depth);
    }
    
    new->tail = PersistentVectorNode_allocate(1, leafNode);
    new->tail->array[0] = other;
  }
  else {
    new = PersistentVector_allocate();
    /* create allocates a tail, but we do not need it since we copy tail this way or the other. */
    memcpy(new, self, sizeof(PersistentVector));
    new->count++;    
    if (self->tail->count < RRB_BRANCHING) {
      if(self->root) retain(self->root); 
      new->tail = PersistentVectorNode_allocate(self->tail->count + 1, leafNode);
      memcpy(new->tail, self->tail, sizeof(PersistentVectorNode) + self->tail->count * sizeof(Object *));
      new->tail->array[self->tail->count] = other;
      new->tail->count++;
      for(int i=0; i<new->tail->count - 1; i++) retain(new->tail->array[i]);
      release(self);
      return new;
    }
    /* The tail was full if we reached this place */
    
    new->tail = PersistentVectorNode_allocate(1, leafNode);
    new->tail->array[0] = other;
  
    BOOL copied;
    if(self->root) retain(self->root);
    retain(self->tail);

    new->root = PersistentVectorNode_pushTail(NULL, self->root,  self->tail, self->shift, &copied); 
    if(!copied && self->root) { 
      new->shift += RRB_BITS;
      
      int depth = 0;
      PersistentVectorNode *c = new->root;
      while(c->type != leafNode) { c = c->array[0]; depth++; }
      printf("--> Allocate! Increasing depth from %llu to %llu, count: %llu, real depth: %d\n", new->shift-RRB_BITS, new->shift, new->count, depth);
      depth = 0;
      c = new->root;
      while(c->type != leafNode) { c = c->array[c->count -1]; depth++; }
      printf ("!!! Right depth: %d\n", depth);
    }
    
  }

  return new;
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
    void *retVal = self->tail->array[index - tailOffset];
    retain(retVal);
    release(self);
    return retVal;
  }

  PersistentVectorNode *node = self->root;
  printf("NTH: level %llu\n", self->shift);
  for(uint64_t level = self->shift; level > 0; level -= RRB_BITS) {
    printf("Descending!\n");
    node = node->array[(index >> level) & RRB_MASK];
  }
  void *retVal = node->array[index & RRB_MASK];
  retain(retVal);
  release(self);
  return retVal;
}

/* mem done */
void* PersistentVector_dynamic_nth(PersistentVector * restrict self, void *indexObject) {
  /* TODO: should throw */
  /* TODO: we should support big integers here as well */
  if(super(index)->type != integerType) {
    release(self);
    release(indexObject);
    return NULL;
  }
  uint64_t index = ((Integer *)indexObject)->value;
  release(indexObject);
  return PersistentVector_nth(self, index);
}



