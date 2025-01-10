#include "Object.h"
#include "PersistentVectorNode.h"
#include "PersistentVector.h"
#include "Transient.h"

/* mem done */
PersistentVectorNode* PersistentVectorNode_allocate(uint64_t count, NodeType type, uint64_t transientID) { 
  /* Why do we allocate full RRB_BRANCHING elements for leaf nodes? 
     Should we optimise and keep only as much as is needed if 
     we copy them when adding new items anyway? 
     
     Is this an optimisation to allow easier reuse with isReusable? Probably yes. 

     If so, is it an acceptable tradeoff to have some size overhead but then 
     faster execution because allocations are not that often needed?

  */
  // Putting count here saves memory but precludes any reuse. 
  // The current implementation does reuse, so anything else than RRB_BRANCHING causes memory corruption
  size_t allocs = RRB_BRANCHING; // type == leafNode ? RRB_BRANCHING : count;
  PersistentVectorNode *self = (PersistentVectorNode *)allocate(sizeof(PersistentVectorNode) + allocs * sizeof(Object *)); 
  self->type = type;
  self->count = count;
  self->transientID = transientID;
  Object_create((Object *)self, persistentVectorNodeType); 
  return self;
}

/* outside refcount system */
BOOL PersistentVectorNode_equals(PersistentVectorNode * restrict self, PersistentVectorNode * restrict other) {
  if (self->count != other->count) return FALSE;
  for(uint64_t i=0; i < self->count; i++) if (!Object_equals(self->array[i], other->array[i])) return FALSE;
  return TRUE;
}

/* outside refcount system */
uint64_t PersistentVectorNode_hash(PersistentVectorNode * restrict self) {
  uint64_t h = 5381;  
  for(uint64_t i=0; i< self->count; i++) h = combineHash(h, Object_hash(self->array[i])); 
  return h;
}

/* mem done */
String *PersistentVectorNode_toString(PersistentVectorNode * restrict self) {
  String *retVal = String_create("");
  String *space = String_create(" ");

  for(uint64_t i=0; i< self->count; i++) {
    Object_retain(self->array[i]);
    String *s = Object_toString(self->array[i]);
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
  for(uint64_t i=0; i<self->count; i++) Object_release(self->array[i]);
}

/* mem done */
PersistentVectorNode *PersistentVectorNode_replacePath(PersistentVectorNode * restrict self, uint64_t level, uint64_t index, Object * restrict other, BOOL allowsReuse, uint64_t vectorTransientID) {
  uint64_t level_index = (index >> level) & RRB_MASK;
  BOOL reusable = allowsReuse && (isReusable(self) || (self->transientID == vectorTransientID));
  PersistentVectorNode *new = NULL;
  if(reusable) new = self;
  else {
    new = PersistentVectorNode_allocate(self->count, self->type, vectorTransientID);
    memcpy(((Object *)new) + 1, ((Object *)self) + 1, sizeof(PersistentVectorNode) - sizeof(Object) + self->count * sizeof(Object *));
  }

  for(uint64_t i=0; i< self->count; i++) {
    if (i == level_index) {
      if (self->type == leafNode) {
        if(reusable) Object_release(new->array[i]);
        new->array[i] = other;
      } else {
        new->array[i] = (Object *)PersistentVectorNode_replacePath((PersistentVectorNode *)new->array[i], level - RRB_BITS, index, other, reusable, vectorTransientID);
      }
    } else {
      if(!reusable) Object_retain(self->array[i]);
    }
  }
  return new;
}

/* mem done */
PersistentVectorNode *PersistentVectorNode_pushTail(PersistentVectorNode * restrict parent, PersistentVectorNode * restrict self, PersistentVectorNode * restrict tailToPush, int32_t level, BOOL *copied, BOOL allowsReuse, uint64_t vectorTransientID) {
  if (self == NULL) { 
    /* Special case, we have no root in the vector */
    *copied = FALSE;
    return tailToPush;
  }

  if(self->type == leafNode) {
    /* Special case, just a single leaf node */
    PersistentVectorNode *new = PersistentVectorNode_allocate(2, internalNode, vectorTransientID);
    new->array[0] = (Object *)self;
    new->array[1] = (Object *)tailToPush;
    *copied = FALSE;
    return new;
  }
  
  PersistentVectorNode *entry = level <= RRB_BITS ? NULL : (PersistentVectorNode *)self->array[self->count - 1];

  BOOL reusable = allowsReuse && (isReusable(self) || (self->transientID == vectorTransientID));
  if(entry) retain(entry);
  BOOL copiedInSubtree;
  PersistentVectorNode *subtree = PersistentVectorNode_pushTail(self, entry, tailToPush, level -= RRB_BITS, &copiedInSubtree, reusable, vectorTransientID);
  
  if(copiedInSubtree) {
    if(reusable) {
      Object_release(self->array[self->count - 1]);
      self->array[self->count - 1] = (Object *)subtree;
      *copied = TRUE;
      return self;
    }
    
    PersistentVectorNode *new = PersistentVectorNode_allocate(self->count, internalNode, vectorTransientID);
    memcpy(((Object *)new)+1, ((Object *)self)+1, sizeof(PersistentVectorNode) - sizeof(Object) + self->count * sizeof(PersistentVectorNode *));    
    new->array[new->count - 1] = (Object *)subtree;
    for (uint64_t i=0; i< new->count - 1; i++) Object_retain(new->array[i]);
    *copied = TRUE;
    release(self);
    return new;
  }
  
  /* We have created a new node somewhere down there */

  if (self->count < RRB_BRANCHING) {
    PersistentVectorNode *new = PersistentVectorNode_allocate(self->count + 1, internalNode, vectorTransientID);
    memcpy(((Object *)new)+1, ((Object *)self)+1, sizeof(PersistentVectorNode) - sizeof(Object) + self->count * sizeof(PersistentVectorNode *));    
    new->array[self->count] = (Object *)subtree;
    new->count++;
    for(uint64_t i=0; i < new->count - 1; i++) Object_retain(new->array[i]); 
    *copied = TRUE;
    release(self);

    return new;
  }

  /* We cannot attach, let us create a new node */

  if(parent == NULL) { 
    /* Root node, we create a new root and merge */
    PersistentVectorNode *new = PersistentVectorNode_allocate(2, internalNode, vectorTransientID);
    new->array[0] = (Object *)self;
    PersistentVectorNode *newDown = PersistentVectorNode_allocate(1, internalNode, vectorTransientID);
    new->array[1] = (Object *)newDown;
    newDown->array[0] = (Object *)subtree;
    *copied = FALSE;
    return new;
  }

  PersistentVectorNode *new = PersistentVectorNode_allocate(1, internalNode, vectorTransientID);
  new->array[0] = (Object *)subtree;
  *copied = FALSE;
  release(self);
    
  return new;
} 

PersistentVectorNode *PersistentVectorNode_popTail(PersistentVectorNode * restrict self, PersistentVectorNode ** restrict poppedLeaf, BOOL allowsReuse, uint64_t vectorTransientID) {
  if (self->type == leafNode) {
    retain(self);
    *poppedLeaf = self;
    return NULL;
  }

  // self->type == internalNode
  uint64_t transientID = self->transientID;
  BOOL reusable = allowsReuse && (isReusable(self) || (transientID && (transientID == vectorTransientID)));
  uint64_t lastPos = self->count - 1; // Invariant: count > 0
  PersistentVectorNode *lastChild = (PersistentVectorNode *)self->array[lastPos];
  PersistentVectorNode *newSubtree = PersistentVectorNode_popTail(lastChild, poppedLeaf, reusable, vectorTransientID);
  BOOL modifiedInPlace = lastChild == newSubtree; // compare addresses!
  PersistentVectorNode *newTree;
  if (reusable) {
    newTree = self;
  } else {
    newTree = PersistentVectorNode_allocate(self->count, self->type, vectorTransientID);
    memcpy(((Object *)newTree)+1, ((Object *)self)+1, sizeof(PersistentVectorNode) + - sizeof(Object) + self->count * sizeof(Object *));
    newTree->transientID = vectorTransientID;
    for (uint64_t i = 0; i < lastPos; ++i) Object_retain(newTree->array[i]); // do not retain last object
  }

  if (newSubtree) {
    if (reusable && !modifiedInPlace) release(lastChild);
    newTree->array[lastPos] = (Object *)newSubtree;
    return newTree;
  }
  
  // newSubtree == NULL
  --newTree->count;
  newTree->array[lastPos] = NULL;
  if (reusable) release(lastChild);
  
  if (lastPos) {
    return newTree;
  }
  
  // self->count == 1
  if (!reusable) release(newTree);
  return NULL;
}

BOOL PersistentVectorNode_contains(PersistentVectorNode * restrict self, void * restrict other) {
  BOOL retVal = FALSE;
  if (self->type == leafNode) {
    for (uint64_t i = 0; i < self->count; ++i) {
      if (Object_equals(self->array[i], other)) {
        retVal = TRUE;
        break;
      }
    }
  } else {
    for (uint64_t i = 0; i < self->count; ++i) {
      PersistentVectorNode *child = (PersistentVectorNode *)self->array[i];
      retain(child);
      retain(other);
      if (PersistentVectorNode_contains(child, other)) {
        retVal = TRUE;
        break;
      }
    }
  }
  release(self);
  release(other);
  return retVal;
}
