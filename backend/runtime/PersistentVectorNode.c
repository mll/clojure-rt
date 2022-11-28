#include "Object.h"
#include "PersistentVectorNode.h"
#include "PersistentVector.h"


/* mem done */
PersistentVectorNode* PersistentVectorNode_allocate(uint64_t count, NodeType type) { 
  Object *super = allocate(sizeof(PersistentVectorNode) + sizeof(Object)); 
  PersistentVectorNode *self = (PersistentVectorNode *)(super + 1);
  self->type = type;
  self->count = count;
  Object_create(super, persistentVectorNodeType); 
  return self;
}

/* outside refcount system */
BOOL PersistentVectorNode_equals(PersistentVectorNode * restrict self, PersistentVectorNode * restrict other) {
  if (self->count != other->count) return FALSE;
  for(int i=0; i < self->count; i++) if (!Object_equals(self->array[i], other->array[i])) return FALSE;
  return TRUE;
}

/* outside refcount system */
uint64_t PersistentVectorNode_hash(PersistentVectorNode * restrict self) {
  uint64_t h = 5381;  
  for(int i=0; i< self->count; i++) h = combineHash(h, Object_hash(self->array[i])); 
  return h;
}

/* mem done */
String *PersistentVectorNode_toString(PersistentVectorNode * restrict self) {
  String *retVal = String_create("");
  String *space = String_create(" ");

  for(int i=0; i< self->count; i++) {
    retain(self->array[i]);
    String *s = toString(self->array[i]);
    retVal = String_concat(retVal, s);
    if (i < self->count - 1) { 
      retain(space); 
      retVal = String_concat(retVal, space);
    }
  } 
  release(space);
  release(self);
  return retVal;
}

/* outside refcount system */
void PersistentVectorNode_destroy(PersistentVectorNode * restrict self, BOOL deallocateChildren) {
  for(int i=0; i<self->count; i++) release(self->array[i]);
}

/* mem done */
PersistentVectorNode *PersistentVectorNode_replacePath(PersistentVectorNode * restrict self, uint64_t level, uint64_t index, void * restrict other) {
  uint64_t level_index = (index >> level) & RRB_MASK;
  PersistentVectorNode *new = NULL;
  BOOL reusable = isReusable(self);
  if(reusable) {
    new = self;
    for(int i=0; i< self->count; i++) {
      if (i == level_index) {
        if (self->type == leafNode) {
          release(new->array[i]);
          new->array[i] = other;
        } else {
          new->array[i] = PersistentVectorNode_replacePath(new->array[i], level - RRB_BITS, index, other);
        }
      } 
    }    
  } else {
    new = PersistentVectorNode_allocate(self->count, self->type);
    memcpy(new, self, sizeof(PersistentVectorNode));
    for(int i=0; i< self->count; i++) {
      if (i == level_index) {
        if (self->type == leafNode) {
          new->array[i] = other;
        } else {
          retain(new->array[i]);
          new->array[i] = PersistentVectorNode_replacePath(new->array[i], level - RRB_BITS, index, other);
        }
      } else {
        retain(new->array[i]);
      }
    }
    release(self);
  }
  return new;
}

/* mem done */
PersistentVectorNode *PersistentVectorNode_pushTail(PersistentVectorNode * restrict parent, PersistentVectorNode * restrict self, PersistentVectorNode * restrict tailToPush, int32_t level, BOOL *copied) {
  printf("!!! Entering level %d for self %p\n", level, self);
  if (self == NULL) { 
    /* Special case, we have no root in the vector */
    printf("!!! Just added tail %p\n", tailToPush);
    *copied = FALSE;
    return tailToPush;
  }

  if(self->type == leafNode) {
    /* Special case, just a single leaf node */
    PersistentVectorNode *new = PersistentVectorNode_allocate(2, internalNode);
    new->array[0] = self;
    new->array[1] = tailToPush;
    *copied = FALSE;
    printf("!!! Just repositioned %p tail %p new parent %p\n", self, tailToPush, new);
    return new;
  }

  PersistentVectorNode *entry = level <= RRB_BITS ? NULL : self->array[self->count - 1];
  printf("!!! Descending from level %d self %p entry %p tail %p\n", level, self, entry, tailToPush);
  BOOL copiedInSubtree;
  // TODO - czy tu walimy retain na entry? 
  if(entry) retain(entry);     
  PersistentVectorNode *subtree = PersistentVectorNode_pushTail(self, entry, tailToPush, level -= RRB_BITS, &copiedInSubtree);
  
  if(copiedInSubtree || self->count < RRB_BRANCHING) {
    PersistentVectorNode *new = NULL;
    BOOL reusable = isReusable(self);
    if(reusable) new = self;
    else {
      new = PersistentVectorNode_allocate(self->count, internalNode);
      memcpy(new, self, sizeof(PersistentVectorNode));    
    }
    
    if(copiedInSubtree) {
      printf("!!! Copied - REUSING %d %p %p\n", reusable, new,  subtree);
      new->array[new->count - 1] = subtree;
      
      if(!reusable) {
        for (int i=0; i< new->count - 1; i++) retain(new->array[i]);
        release(self);
      }
      *copied = TRUE;
      return new;
    }
    
    /* We have created a new node somewhere down there */
    
    if (self->count < RRB_BRANCHING) {
      printf("!!! Attaching - REUSING %d %p %p\n", reusable, new,  subtree);
      new->array[self->count] = subtree;
      new->count++;
      if(!reusable) {
        for(int i=0; i < self->count - 1; i++) retain(new->array[i]); 
        release(self);
      }
      *copied = TRUE;
      return new;
    }
  }
  
  /* We cannot attach, let us create a new node */
  
  if(parent == NULL) { 
    /* Root node, we create a new root and merge */
    PersistentVectorNode *new = PersistentVectorNode_allocate(2, internalNode);
    new->array[0] = self;
    PersistentVectorNode *newDown = PersistentVectorNode_allocate(1, internalNode);
    new->array[1] = newDown;
    newDown->array[0] = subtree;
    *copied = FALSE;
    printf("!!! Creating new down %p %p %p\n", new, newDown, subtree);
    return new;
  }


  PersistentVectorNode *new = PersistentVectorNode_allocate(1, internalNode);
  new->array[0] = subtree;
  *copied = FALSE;
  printf("!!! Creating single subtree %p %p\n", new,  subtree);
  release(self);
  return new;
} 

