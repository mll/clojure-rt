#include "Object.h"
#include "PersistentVectorNode.h"
#include "PersistentVector.h"


PersistentVectorNode* PersistentVectorNode_allocate(uint64_t count, NodeType type) { 
  Object *super = allocate(sizeof(PersistentVectorNode)+ sizeof(Object) + count * sizeof(PersistentVectorNode *)); 
  PersistentVectorNode *self = (PersistentVectorNode *)(super + 1);
  self->type = type;
  self->count = count;
  Object_create(super, persistentVectorNodeType); 
  return self;
}

BOOL PersistentVectorNode_equals(PersistentVectorNode * restrict self, PersistentVectorNode * restrict other) {
  if (self->count != other->count) return FALSE;
  for(int i=0; i < self->count; i++) if (!Object_equals(self->array[i], other->array[i])) return FALSE;
  return TRUE;
}

uint64_t PersistentVectorNode_hash(PersistentVectorNode * restrict self) {
  uint64_t h = 5381;  
  for(int i=0; i< self->count; i++) h = combineHash(h, Object_hash(self->array[i])); 
  return h;
}

String *PersistentVectorNode_toString(PersistentVectorNode * restrict self) {
  String *retVal = String_create("");
  String *space = String_create(" ");

  for(int i=0; i< self->count; i++) {
    Object_retain(self->array[i]);
    String *s = Object_toString(self->array[i]);
    retVal = String_concat(retVal, s);
    if (i < self->count - 1) {
      retain(space);
      retVal = String_concat(retVal, space);
    }
  } 
  release(space);
  return retVal;
}

void PersistentVectorNode_destroy(PersistentVectorNode * restrict self, BOOL deallocateChildren) {
  for(int i=0; i<self->count; i++) Object_release(self->array[i]);
}


PersistentVectorNode *PersistentVectorNode_replacePath(PersistentVectorNode * restrict self, uint64_t level, uint64_t index, Object * restrict other, BOOL allowsReuse) {
  uint64_t level_index = (index >> level) & RRB_MASK;
  BOOL reusable = isReusable(self) && allowsReuse;
  PersistentVectorNode *new = NULL;
  if(reusable) new = self;
  else {
    new = PersistentVectorNode_allocate(self->count, self->type);
    memcpy(new, self, sizeof(PersistentVectorNode) + self->count * sizeof(Object *));
  }

  for(int i=0; i< self->count; i++) {
    if (i == level_index) {
      if (self->type == leafNode) {
        if(reusable) Object_release(new->array[i]);
        new->array[i] = other;
      } else {
        new->array[i] = super(PersistentVectorNode_replacePath(Object_data(new->array[i]), level - RRB_BITS, index, other, reusable));
      }
    } else {
      if(!reusable) Object_retain(self->array[i]);
    }
  }
  return new;
}

PersistentVectorNode *PersistentVectorNode_pushTail(PersistentVectorNode * restrict parent, PersistentVectorNode * restrict self, PersistentVectorNode * restrict tailToPush, int32_t level, BOOL *copied) {
  if (self == NULL) { 
    /* Special case, we have no root in the vector */
    retain(tailToPush);
    *copied = FALSE;
    return tailToPush;
  }

  if(self->type == leafNode) {
    /* Special case, just a single leaf node */
     PersistentVectorNode *new = PersistentVectorNode_allocate(2, internalNode);
    retain(self);
    retain(tailToPush);
    new->array[0] = super(self);
    new->array[1] = super(tailToPush);
    *copied = FALSE;
    return new;
  }

  PersistentVectorNode *entry = level <= RRB_BITS ? NULL : Object_data(self->array[self->count - 1]);

  BOOL copiedInSubtree;
  PersistentVectorNode *subtree = PersistentVectorNode_pushTail(self, entry, tailToPush, level -= RRB_BITS, &copiedInSubtree);
  
  if(copiedInSubtree) {
    PersistentVectorNode *new = PersistentVectorNode_allocate(self->count, internalNode);
    memcpy(new, self, sizeof(PersistentVectorNode) + self->count * sizeof(PersistentVectorNode *));    
    new->array[new->count - 1] = super(subtree);
    for (int i=0; i< new->count - 1; i++) Object_retain(new->array[i]);
    *copied = TRUE;
    return new;
  }
  
  /* We have created a new node somewhere down there */

  if (self->count < RRB_BRANCHING) {
    PersistentVectorNode *new = PersistentVectorNode_allocate(self->count + 1, internalNode);
    memcpy(new, self, sizeof(PersistentVectorNode) + self->count * sizeof(PersistentVectorNode *));
    new->array[self->count] = super(subtree);
    new->count++;
    for(int i=0; i < new->count - 1; i++) Object_retain(new->array[i]); 
    *copied = TRUE;
    return new;
  }

  /* We cannot attach, let us create a new node */

  if(parent == NULL) { 
    /* Root node, we create a new root and merge */
    PersistentVectorNode *new = PersistentVectorNode_allocate(2, internalNode);
    retain(self);
    new->array[0] = super(self);
    PersistentVectorNode *newDown = PersistentVectorNode_allocate(1, internalNode);
    new->array[1] = super(newDown);
    newDown->array[0] = super(subtree);
    *copied = FALSE;
    return new;
  }

  PersistentVectorNode *new = PersistentVectorNode_allocate(1, internalNode);
  new->array[0] = super(subtree);
  *copied = FALSE;
  return new;
} 

