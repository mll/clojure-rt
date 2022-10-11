#include "PersistentVectorNode.h"
#include "PersistentVector.h"

PersistentVectorNode* PersistentVectorNode_create(uint64_t count, NodeType type) { 
  Object *super = allocate(sizeof(PersistentVectorNode)+ sizeof(Object) + count * sizeof(PersistentVectorNode *)); 
  PersistentVectorNode *self = (PersistentVectorNode *)(super + 1);
  self->type = type;
  self->count = count;
  Object_create(super, persistentVectorNodeType); 
  return self;
}

bool PersistentVectorNode_equals(PersistentVectorNode *self, PersistentVectorNode *other) {
  if (self->count != other->count) return false;
  for(int i=0; i < self->count; i++) if (!equals(self->array[i], other->array[i])) return false;
  return true;
}

uint64_t PersistentVectorNode_hash(PersistentVectorNode *self) {
  uint64_t h = 5381;
  for(int i=0; i< self->count; i++) h = ((h << 5) + h) + hash(self->array[i]);
  return h;
}

String *PersistentVectorNode_toString(PersistentVectorNode *self) {
  sds retVal = sdsnew("");
  for(int i=0; i< self->count; i++) {
    String *s = toString(self->array[i]);
    retVal = sdscat(retVal, s->value);
    if (i < self->count - 1) retVal = sdscat(retVal, " ");
    release(s);
  } 
  return String_create(retVal);
}

void PersistentVectorNode_destroy(PersistentVectorNode *self, bool deallocateChildren) {
  for(int i=0; i<self->count; i++) Object_release(self->array[i]);
}


PersistentVectorNode *PersistentVectorNode_replacePath(PersistentVectorNode *self, uint64_t level, uint64_t index, Object *other) {
  uint64_t level_index = (index >> level) & RRB_MASK;
  PersistentVectorNode *new = PersistentVectorNode_create(self->count, self->type);
  memcpy(new, self, sizeof(PersistentVectorNode) + self->count * sizeof(Object *));
  for(int i=0; i< self->count; i++) {
    if (i == level_index) {
      if (self->type == leafNode) {
        Object_retain(other);
        new->array[i] = other;
      } else {
        new->array[i] = super(PersistentVectorNode_replacePath(data(new->array[i]), level - RRB_BITS, index, other));
      }
    } else {
      Object_retain(self->array[i]);
    }
  }
  return new;
}

PersistentVectorNode *PersistentVectorNode_pushTail(PersistentVectorNode *parent, PersistentVectorNode *self, PersistentVectorNode *tailToPush, int32_t level, bool *copied) {
  if (self == NULL) { 
    /* Special case, we have no root in the vector */
    retain(tailToPush);
    *copied = false;
    return tailToPush;
  }

  if(self->type == leafNode) {
    /* Special case, just a single leaf node */
     PersistentVectorNode *new = PersistentVectorNode_create(2, internalNode);
    retain(self);
    retain(tailToPush);
    new->array[0] = super(self);
    new->array[1] = super(tailToPush);
    *copied = false;
    return new;
  }

  PersistentVectorNode *entry = level <= RRB_BITS ? NULL : data(self->array[self->count - 1]);

  bool copiedInSubtree;
  PersistentVectorNode *subtree = PersistentVectorNode_pushTail(self, entry, tailToPush, level -= RRB_BITS, &copiedInSubtree);
  
  if(copiedInSubtree) {
    PersistentVectorNode *new = PersistentVectorNode_create(self->count, internalNode);
    memcpy(new, self, sizeof(PersistentVectorNode) + self->count * sizeof(PersistentVectorNode *));    
    new->array[new->count - 1] = super(subtree);
    for (int i=0; i< new->count - 1; i++) Object_retain(new->array[i]);
    *copied = true;
    return new;
  }
  
  /* We have created a new node somewhere down there */

  if (self->count < RRB_BRANCHING) {
    PersistentVectorNode *new = PersistentVectorNode_create(self->count + 1, internalNode);
    memcpy(new, self, sizeof(PersistentVectorNode) + self->count * sizeof(PersistentVectorNode *));
    new->array[self->count] = super(subtree);
    new->count++;
    for(int i=0; i < new->count - 1; i++) Object_retain(new->array[i]); 
    *copied = true;
    return new;
  }

  /* We cannot attach, let us create a new node */

  if(parent == NULL) { 
    /* Root node, we create a new root and merge */
    PersistentVectorNode *new = PersistentVectorNode_create(2, internalNode);
    retain(self);
    new->array[0] = super(self);
    PersistentVectorNode *newDown = PersistentVectorNode_create(1, internalNode);
    new->array[1] = super(newDown);
    newDown->array[0] = super(subtree);
    *copied = false;
    return new;
  }

  PersistentVectorNode *new = PersistentVectorNode_create(1, internalNode);
  new->array[0] = super(subtree);
  *copied = false;
  return new;
} 

