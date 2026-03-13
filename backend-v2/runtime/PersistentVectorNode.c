#include "Object.h"
#include "PersistentVectorNode.h"
#include "PersistentVector.h"
#include "RTValue.h"
#include "Transient.h"
#include "Exceptions.h"

/* mem done */
PersistentVectorNode* PersistentVectorNode_allocate(uword_t count, NodeType type, uword_t transientID) { 
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
  PersistentVectorNode *self = (PersistentVectorNode *)allocate(sizeof(PersistentVectorNode) + allocs * sizeof(RTValue)); 
  self->type = type;
  self->count = count;
  self->transientID = transientID;
  Object_create((Object *)self, persistentVectorNodeType); 
  return self;
}

/* outside refcount system */
bool PersistentVectorNode_equals(PersistentVectorNode * restrict self, PersistentVectorNode * restrict other) {
  if (self->count != other->count) return false;
  for(uword_t i=0; i < self->count; i++) if (!equals(self->array[i], other->array[i])) return false;
  return true;
}

/* outside refcount system */
uword_t PersistentVectorNode_hash(PersistentVectorNode * restrict self) {
  uword_t h = 5381;  
  for(uword_t i=0; i< self->count; i++) h = combineHash(h, hash(self->array[i])); 
  return h;
}

/* mem done */
String *PersistentVectorNode_toString(PersistentVectorNode * restrict self) {
  String *retVal = String_create("");
  String *space = String_create(" ");

  for(uword_t i=0; i< self->count; i++) {
    retain(self->array[i]);
    String *s = toString(self->array[i]);
    retVal = String_concat(retVal, s);
    if (i < self->count - 1) {
      Ptr_retain(space);
      retVal = String_concat(retVal, space);
    }
  } 
  Ptr_release(space);
  Ptr_release(self);
  return retVal;
}

/* outside refcount system */
void PersistentVectorNode_destroy(PersistentVectorNode * restrict self, bool deallocateChildren) {
  for(uword_t i=0; i<self->count; i++) release(self->array[i]);
}

void PersistentVectorNode_promoteToShared(PersistentVectorNode *self,
                                          uword_t current) {
  if (current & SHARED_BIT)
    return;

  if (self->transientID != 0) {
    throwIllegalStateException_C("Cannot share a transient vector node");
  }

  if (self->type == leafNode) {
    for (uword_t i = 0; i < self->count; i++) {
      promoteToShared(self->array[i]);
    }
  } else {
    for (uword_t i = 0; i < self->count; i++) {
      promoteToShared(self->array[i]);
    }
  }
  Object_promoteToSharedShallow((Object *)self, current);
}

/* mem done */
PersistentVectorNode *PersistentVectorNode_replacePath(PersistentVectorNode * restrict self, uword_t level, uword_t index, RTValue other, bool allowsReuse, uword_t vectorTransientID) {
  uword_t level_index = (index >> level) & RRB_MASK;
  bool reusable = allowsReuse && (Ptr_isReusable(self) || (self->transientID == vectorTransientID));
  PersistentVectorNode *new = NULL;
  
  if(reusable) new = self;
  else {
    new = PersistentVectorNode_allocate(self->count, self->type, vectorTransientID);
    memcpy(((Object *)new) + 1, ((Object *)self) + 1, sizeof(PersistentVectorNode) - sizeof(Object) + self->count * sizeof(RTValue));
  }

  for(uword_t i=0; i< self->count; i++) {
    if (i == level_index) {
      if (self->type == leafNode) {
        if(reusable) release(new->array[i]);
        new->array[i] = other;
      } else {
        new->array[i] = RT_boxPtr(PersistentVectorNode_replacePath((PersistentVectorNode *)RT_unboxPtr(new->array[i]), level - RRB_BITS, index, other, reusable, vectorTransientID));
      }
    } else {
      if(!reusable) retain(self->array[i]);
    }
  }
  return new;
}

/* mem done */
PersistentVectorNode *PersistentVectorNode_pushTail(PersistentVectorNode * restrict parent, PersistentVectorNode * restrict self, PersistentVectorNode * restrict tailToPush, int32_t level, bool *copied, bool allowsReuse, uword_t vectorTransientID) {
  if (self == NULL) { 
    /* Special case, we have no root in the vector */
    *copied = false;
    return tailToPush;
  }

  if(self->type == leafNode) {
    /* Special case, just a single leaf node */
    PersistentVectorNode *new = PersistentVectorNode_allocate(2, internalNode, vectorTransientID);
    new->array[0] = RT_boxPtr(self);
    new->array[1] = RT_boxPtr(tailToPush);
    *copied = false;
    return new;
  }
  
  PersistentVectorNode *entry = level <= RRB_BITS ? NULL : (PersistentVectorNode *)RT_unboxPtr(self->array[self->count - 1]);

  bool reusable = allowsReuse && (Ptr_isReusable(self) || (self->transientID == vectorTransientID));
  if(entry) retain(RT_boxPtr(entry));
  bool copiedInSubtree;
  PersistentVectorNode *subtree = PersistentVectorNode_pushTail(self, entry, tailToPush, level -= RRB_BITS, &copiedInSubtree, reusable, vectorTransientID);
  
  if(copiedInSubtree) {
    if(reusable) {
      release(self->array[self->count - 1]);
      self->array[self->count - 1] = RT_boxPtr(subtree);
      *copied = true;
      return self;
    }
    
    PersistentVectorNode *new = PersistentVectorNode_allocate(self->count, internalNode, vectorTransientID);
    memcpy(((Object *)new)+1, ((Object *)self)+1, sizeof(PersistentVectorNode) - sizeof(Object) + self->count * sizeof(RTValue));    
    new->array[new->count - 1] = RT_boxPtr(subtree);
    for (uword_t i=0; i< new->count - 1; i++) retain(new->array[i]);
    *copied = true;
    Ptr_release(self);
    return new;
  }
  
  /* We have created a new node somewhere down there */

  if (self->count < RRB_BRANCHING) {
    PersistentVectorNode *new = PersistentVectorNode_allocate(self->count + 1, internalNode, vectorTransientID);
    memcpy(((Object *)new)+1, ((Object *)self)+1, sizeof(PersistentVectorNode) - sizeof(Object) + self->count * sizeof(RTValue));    
    new->array[self->count] = RT_boxPtr(subtree);
    new->count++;
    for(uword_t i=0; i < new->count - 1; i++) retain(new->array[i]); 
    *copied = true;
    Ptr_release(self);

    return new;
  }

  /* We cannot attach, let us create a new node */

  if(parent == NULL) { 
    /* Root node, we create a new root and merge */
    PersistentVectorNode *new = PersistentVectorNode_allocate(2, internalNode, vectorTransientID);
    new->array[0] = RT_boxPtr(self);
    PersistentVectorNode *newDown = PersistentVectorNode_allocate(1, internalNode, vectorTransientID);
    new->array[1] = RT_boxPtr(newDown);
    newDown->array[0] = RT_boxPtr(subtree);
    *copied = false;
    return new;
  }

  PersistentVectorNode *new = PersistentVectorNode_allocate(1, internalNode, vectorTransientID);
  new->array[0] = RT_boxPtr(subtree);
  *copied = false;
  Ptr_release(self);
    
  return new;
} 

PersistentVectorNode *PersistentVectorNode_popTail(PersistentVectorNode * restrict self, PersistentVectorNode ** restrict poppedLeaf, bool allowsReuse, uword_t vectorTransientID) {
  if (self->type == leafNode) {
    Ptr_retain(self);
    *poppedLeaf = self;
    return NULL;
  }

  // self->type == internalNode
  uword_t transientID = self->transientID;
  bool reusable = allowsReuse && (Ptr_isReusable(self) || (transientID && (transientID == vectorTransientID)));
  uword_t lastPos = self->count - 1; // Invariant: count > 0
  PersistentVectorNode *lastChild = (PersistentVectorNode *)RT_unboxPtr(self->array[lastPos]);
  PersistentVectorNode *newSubtree = PersistentVectorNode_popTail(lastChild, poppedLeaf, reusable, vectorTransientID);
  bool modifiedInPlace = lastChild == newSubtree; // compare addresses!
  PersistentVectorNode *newTree;
  if (reusable) {
    newTree = self;
  } else {
    newTree = PersistentVectorNode_allocate(self->count, self->type, vectorTransientID);
    memcpy(((Object *)newTree)+1, ((Object *)self)+1, sizeof(PersistentVectorNode) + - sizeof(Object) + self->count * sizeof(RTValue));
    newTree->transientID = vectorTransientID;
    for (uword_t i = 0; i < lastPos; ++i) retain(newTree->array[i]); // do not retain last object
  }

  if (newSubtree) {
    if (reusable && !modifiedInPlace) Ptr_release(lastChild);
    newTree->array[lastPos] = RT_boxPtr(newSubtree);
    return newTree;
  }
  
  // newSubtree == NULL
  --newTree->count;
// This is not necessary, as the count explains what is and what is not present.  
//  newTree->array[lastPos] = RT_boxNull();
  if (reusable) Ptr_release(lastChild);
  
  if (lastPos) {
    return newTree;
  }
  
  // self->count == 1
  if (!reusable) Ptr_release(newTree);
  return NULL;
}

bool PersistentVectorNode_contains(PersistentVectorNode * restrict self, RTValue other) {
  bool retVal = false;
  if (self->type == leafNode) {
    for (uword_t i = 0; i < self->count; ++i) {
      if (equals(self->array[i], other)) {
        retVal = true;
        break;
      }
    }
  } else {
    for (uword_t i = 0; i < self->count; ++i) {
      PersistentVectorNode *child = (PersistentVectorNode *)RT_unboxPtr(self->array[i]);
      Ptr_retain(child);
      retain(other);
      if (PersistentVectorNode_contains(child, other)) {
        retVal = true;
        break;
      }
    }
  }
  Ptr_release(self);
  release(other);
  return retVal;
}
