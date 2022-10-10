#include "PersistentVector.h"
#include "Object.h"

#define RRB_BITS 5
#define RRB_MAX_HEIGHT 7

#define RRB_BRANCHING (1 << RRB_BITS)
#define RRB_MASK (RRB_BRANCHING - 1)

#define RRB_INVARIANT 1
#define RRB_EXTRAS 2


#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

// Typical stuff
#define RRB_SHIFT(rrb) (rrb->shift)
#define INC_SHIFT(shift) (shift + (uint32_t) RRB_BITS)
#define DEC_SHIFT(shift) (shift - (uint32_t) RRB_BITS)
#define LEAF_NODE_SHIFT ((uint32_t) 0)

// Abusing allocated pointers being unique to create GUIDs: using a single
// malloc to create a guid.
#define GUID_DECLARATION const void *guid;

typedef enum {LEAF_NODE, INTERNAL_NODE} NodeType;

typedef struct PersistentVectorNode PersistentVectorNode;
typedef struct PersistentVectorLeafNode PersistentVectorLeafNode;
typedef struct PersistentVectorInternalNode PersistentVectorInternalNode;
typedef struct RRBSizeTable RRBSizeTable;

struct PersistentVectorNode {
  NodeType type;
  uint32_t len;
  GUID_DECLARATION;
};

struct PersistentVectorLeafNode {
  NodeType type;
  uint32_t len;
  GUID_DECLARATION;
  Object* child[];
};

struct PersistentVectorInternalNode {
  NodeType type;
  uint32_t len;
  GUID_DECLARATION;
  RRBSizeTable *size_table;
  PersistentVectorInternalNode *child[];
};

struct RRBSizeTable {
  GUID_DECLARATION
  uint32_t size[];   
};


static PersistentVectorLeafNode *EMPTY_LEAF = NULL;
static PersistentVector *EMPTY_RRB = NULL;

PersistentVector* PersistentVector_tailAppend( PersistentVector *rrb,  Object *elt);
PersistentVector* PersistentVector_pushTailDown(PersistentVector *rrb, PersistentVector *new_rrb, PersistentVectorLeafNode *new_tail);


static uint32_t sized_pos(const InternalNode *node, uint32_t *index,
                          uint32_t sp) {
  RRBSizeTable *table = node->size_table;
  uint32_t is = *index >> sp;
  while (table->size[is] <= *index) {
    is++;
  }
  if (is != 0) {
    *index -= table->size[is-1];
  }
  return is;
}

static const InternalNode* sized(const InternalNode *node, uint32_t *index,
                                 uint32_t sp) {
  uint32_t is = sized_pos(node, index, sp);
  return (InternalNode *) node->child[is];
}

Object* PersistentVector_nth(PersistentVector *rrb, uint32_t index) {
  if (index >= rrb->count || index < 0) {
    return NULL;
  }

  uint32_t tail_offset = rrb->count - rrb->tail_len;
  if (tail_offset <= index) {
    return rrb->tail->child[index - tail_offset];
  }
  else {
    PersistentVectorInternalNode *current = (PersistentVectorInternalNode *) rrb->root;
    for (uint32_t shift = RRB_SHIFT(rrb); shift > 0; shift -= RRB_BITS) {
      if (current->size_table == NULL) {
        const uint32_t subidx = (index >> shift) & RRB_MASK;
        current = current->child[subidx];
      }
      else {
        current = sized(current, &index, shift);
      }
    }
    return ((PersistentVectorLeafNode *)current)->child[index & RRB_MASK];
  }
}









RRBSizeTable* RRBSizeTable_create(uint32_t size) {
  return allocate(sizeof(RRBSizeTable) + size * sizeof(uint32_t));
}

RRBSizeTable* RRBSizeTable_clone( RRBSizeTable *original, uint32_t len, uint32_t extra) {
  RRBSizeTable *clone = allocate(sizeof(RRBSizeTable) + (len + extra)* sizeof(uint32_t));
  memcpy(&clone->size, &original->size, sizeof(uint32_t) * len);
  return clone;
}

PersistentVectorInternalNode* PersistentVectorInternalNode_create(uint32_t len) {  
  Object *super = allocate(sizeof(PersistentVectorInternalNode)+ sizeof(Object) + len * sizeof(PersistentVectorInternalNode *)); 
  PersistentVectorInternalNode *self = (PersistentVectorInternalNode *)(super + 1);
  self->type = INTERNAL_NODE;
  self->len = len;
  self->size_table = NULL;
  Object_create(super, persistentVectorNodeType); 
  return self;
}


PersistentVectorLeafNode* PersistentVectorLeafNode_create(uint32_t len) {
  if (EMPTY_LEAF && len == 0) {
    retain(EMPTY_LEAF);
    return EMPTY_LEAF;
  }
  Object *super = allocate(sizeof(PersistentVectorLeafNode)+ sizeof(Object) + len * sizeof(Object *)); 
  PersistentVectorLeafNode *self = (PersistentVectorLeafNode *)(super + 1);
  self->type = LEAF_NODE;
  self->len = len;
  Object_create(super, persistentVectorNodeType);
  if(len == 0) { 
    EMPTY_LEAF = self;
    retain(self);
  }
  return self;
}

/* Vector */

PersistentVector* PersistentVector_create() {
  if (EMPTY_RRB) { 
    retain(EMPTY_RRB);
    return EMPTY_RRB; 
  }
  
  Object *super = allocate(sizeof(PersistentVector)+ sizeof(Object)); 
  PersistentVector *self = (PersistentVector *)(super + 1);
  self->count = 0;
  self->shift = 0;
  self->root = NULL;
  self->tail_len = 0;
  self->tail = PersistentVectorLeafNode_create(0);
  Object_create(super, persistentVectorType);
  EMPTY_RRB = self;
  retain(self);
  return self;
}

bool PersistentVector_equals(PersistentVector *self, PersistentVector *other) {
  return false;
}
uint64_t PersistentVector_hash(PersistentVector *self) {
  return 0;
}
String *PersistentVector_toString(PersistentVector *self) {
  return NULL;
}
void PersistentVector_destroy(PersistentVector *self, bool deallocateChildren) {

}


PersistentVector* PersistentVector_headClone(PersistentVector* original) {
  Object *super = allocate(sizeof(PersistentVector)+ sizeof(Object)); 
  PersistentVector *self = (PersistentVector *)(super + 1);
  Object_create(super, persistentVectorType);
  memcpy(self, original, sizeof(PersistentVector));
  // NOT YET
  //if(self->tail) retain(self->tail);
  //if(self->root) retain(self->root);
  return self;
}


PersistentVector* PersistentVector_conj(PersistentVector *self, Object *other) {
 // NOT YET  Object_retain(other);
  if (self->tail_len < RRB_BRANCHING) {
    return PersistentVector_tailAppend(self, other);
  }
  PersistentVector *new_rrb = PersistentVector_headClone(self);
  new_rrb->count++;

  PersistentVectorLeafNode *new_tail = PersistentVectorLeafNode_create(1);
  new_tail->child[0] = other;
  new_rrb->tail_len = 1;
  return PersistentVector_pushTailDown(self, new_rrb, new_tail);
}

PersistentVectorInternalNode* PersistentVectorInternalNode_clone( PersistentVectorInternalNode *original, int32_t extra) {
  size_t size = sizeof(PersistentVectorInternalNode) + original->len * sizeof(PersistentVectorInternalNode *);
  size_t fullSize = sizeof(Object) + size + sizeof(PersistentVectorInternalNode *) * extra;

  Object *super = allocate(fullSize); 
  Object_create(super, persistentVectorNodeType); 
  PersistentVectorInternalNode *self = (PersistentVectorInternalNode *)(super + 1);
  memcpy(self, original, extra >= 0 ? size : (size + extra * sizeof(PersistentVectorInternalNode *)));

  // NOT YET
  /* int end = extra >= 0 ? self->len : self->len + extra; */
  /* for(int i=0; i < end; i++) { */
  /*   retain(self->child[i]); */
  /* } */

  if (self->size_table != NULL) {
    self->size_table = RRBSizeTable_clone(self->size_table, self->len, extra);
  }

  self->len += extra;
  return self;
}

PersistentVectorLeafNode *PersistentVectorLeafNode_clone(PersistentVectorLeafNode *original, int32_t extra) {
  size_t size = sizeof(PersistentVectorLeafNode) + original->len * sizeof(Object *);
  size_t fullSize = sizeof(Object) + extra * sizeof(Object *) + size;
  Object *super = allocate(fullSize); 
  Object_create(super, persistentVectorNodeType);
  PersistentVectorLeafNode *self = (PersistentVectorLeafNode *)(super + 1);
  memcpy(self, original, extra >= 0 ? size : (size + extra * sizeof(PersistentVectorInternalNode *)));

  // NOT YET
  /* int end = extra >= 0 ? self->len : self->len + extra; */
  /* for(int i=0; i < end; i++) { */
  /*   retain(self->child[i]); */
  /* } */

  self->len += extra;
  return self;
}




// - Height should be shift or height, not max element size
// - copy_first_k returns a pointer to the next pointer to set
// - append_empty now returns a pointer to the *void we're supposed to set

PersistentVectorInternalNode** PersistentVectorInternalNode_copyFirstK( PersistentVector *rrb, PersistentVector *new_rrb,  uint32_t k,  uint32_t tail_size) {
   PersistentVectorInternalNode *current = ( PersistentVectorInternalNode *) rrb->root;
  PersistentVectorInternalNode **to_set = (PersistentVectorInternalNode **) &new_rrb->root;
  uint32_t index = rrb->count - 1;
  uint32_t shift = RRB_SHIFT(rrb);

  // Copy all non-leaf nodes first. Happens when shift > RRB_BRANCHING
  uint32_t i = 1;
  while (i <= k && shift != 0) {
    // First off, copy current node and stick it in.
    PersistentVectorInternalNode *new_current;
    if (i != k) {
      new_current = PersistentVectorInternalNode_clone(current, 0);
      if (new_current->size_table) new_current->size_table->size[new_current->len-1] += tail_size;
    }
    else { // increment size of last elt -- will only happen if we append empties
      new_current = PersistentVectorInternalNode_clone(current, 1);
      if (current->size_table != NULL) {
        new_current->size_table->size[new_current->len-1] =
          new_current->size_table->size[new_current->len-2] + tail_size;
      }
    }
    *to_set = new_current;

    // calculate child index
    uint32_t child_index;
    if (current->size_table == NULL) {
      child_index = (index >> shift) & RRB_MASK;
    }
    else {
      // no need for sized_pos here, luckily.
      child_index = new_current->len - 1;
      // Decrement index
      if (child_index != 0) {
        index -= current->size_table->size[child_index-1];
      }
    }
    to_set = &new_current->child[child_index];
    current = current->child[child_index];

    i++;
    shift -= RRB_BITS;
  }

  return to_set;
}


PersistentVectorInternalNode** PersistentVectorInternalNode_appendEmpty(PersistentVectorInternalNode **to_set, uint32_t empty_height) {
  if (0 < empty_height) {
    PersistentVectorInternalNode *leaf = PersistentVectorInternalNode_create(1);
    PersistentVectorInternalNode *empty = (PersistentVectorInternalNode *) leaf;
    for (uint32_t i = 1; i < empty_height; i++) {
      PersistentVectorInternalNode *new_empty = PersistentVectorInternalNode_create(1);
      new_empty->child[0] = empty;
      empty = new_empty;
    }
    // this root node must be one larger, otherwise segfault
    *to_set = empty;
    return &leaf->child[0];
  }
  else {
    return to_set;
  }
}

PersistentVector* PersistentVector_tailAppend( PersistentVector *rrb,  Object *elt) {
  PersistentVector* new_rrb = PersistentVector_headClone(rrb);
  PersistentVectorLeafNode *new_tail = PersistentVectorLeafNode_clone(rrb->tail, 1);
  // NOT YET
  //Object_retain(elt);
  new_tail->child[new_rrb->tail_len] = elt;
  new_rrb->count++;
  new_rrb->tail_len++;
  new_rrb->tail = new_tail;
  return new_rrb;
}



PersistentVector* PersistentVector_pushTailDown(PersistentVector *rrb, PersistentVector *new_rrb, PersistentVectorLeafNode *new_tail) {
  PersistentVectorLeafNode *old_tail = new_rrb->tail;
  new_rrb->tail = new_tail;
  if (rrb->count <= RRB_BRANCHING) {
    new_rrb->shift = LEAF_NODE_SHIFT;
    new_rrb->root = (PersistentVectorNode *) old_tail;
    return new_rrb;
  }
  // Copyable count starts here

  // TODO: Can find last rightmost jump in ant time for pvec subvecs:
  // use the fact that (index & large_mask) == 1 << (RRB_BITS * H) - 1 -> 0 etc.

  uint32_t index = rrb->count - 1;

  uint32_t nodes_to_copy = 0;
  uint32_t nodes_visited = 0;
  uint32_t pos = 0; // pos is the position we insert empty nodes in the bottom
                    // copyable node (or the element, if we can copy the leaf)
  PersistentVectorInternalNode *current = (PersistentVectorInternalNode *) rrb->root;

  uint32_t shift = RRB_SHIFT(rrb);

  // checking all non-leaf nodes (or if tail, all but the lowest two levels)
  while (shift > INC_SHIFT(LEAF_NODE_SHIFT)) {
    // calculate child index
    uint32_t child_index;
    if (current->type == LEAF_NODE) {
      // some check here to ensure we're not overflowing the pvec subvec.
      // important to realise that this only needs to be done once in a better
      // impl, the same way the size_table check only has to be done until it's
      // false.
       uint32_t prev_shift = shift + RRB_BITS;
      if (index >> prev_shift > 0) {
        nodes_visited++; // this could possibly be done earlier in the code.
        goto copyable_count_end;
      }
      child_index = (index >> shift) & RRB_MASK;
      // index filtering is not necessary when the check above is performed at
      // most once.
      index &= ~(RRB_MASK << shift);
    }
    else {
      // no need for sized_pos here, luckily.
      child_index = current->len - 1;
      // Decrement index
      if (child_index != 0) {
        index -= current->size_table->size[child_index-1];
      }
    }
    nodes_visited++;
    if (child_index < RRB_MASK) {
      nodes_to_copy = nodes_visited;
      pos = child_index;
    }

    current = current->child[child_index];
    // This will only happen in a pvec subtree
    if (current == NULL) {
      nodes_to_copy = nodes_visited;
      pos = child_index;

      // if next element we're looking at is null, we can copy all above. Good
      // times.
      goto copyable_count_end;
    }
    shift -= RRB_BITS;
  }
  // if we're here, we're at the leaf node (or lowest non-leaf), which is
  // `current`

  // no need to even use index here: We know it'll be placed at current->len,
  // if there's enough space. That check is easy.
  if (shift != 0) {
    nodes_visited++;
    if (current->len < RRB_BRANCHING) {
      nodes_to_copy = nodes_visited;
      pos = current->len;
    }
  }

 copyable_count_end:
  // GURRHH, nodes_visited is not yet handled nicely. for loop down to get
  // nodes_visited set straight.
  while (shift > INC_SHIFT(LEAF_NODE_SHIFT)) {
    nodes_visited++;
    shift -= RRB_BITS;
  }

  // Increasing height of tree.
  if (nodes_to_copy == 0) {
    PersistentVectorInternalNode *new_root = PersistentVectorInternalNode_create(2);
    new_root->child[0] = (PersistentVectorInternalNode *) rrb->root;
    new_rrb->root = (PersistentVectorNode *) new_root;
    new_rrb->shift = INC_SHIFT(RRB_SHIFT(new_rrb));

    // create size table if the original rrb root has a size table.
    if (rrb->root->type != LEAF_NODE &&
        ((PersistentVectorInternalNode *)rrb->root)->size_table != NULL) {
      RRBSizeTable *table = RRBSizeTable_create(2);
      table->size[0] = rrb->count - old_tail->len;
      // If we insert the tail, the old size minus the old tail size will be the
      // amount of elements in the left branch. If there is no tail, the size is
      // just the old rrb-tree.

      table->size[1] = rrb->count;
      // If we insert the tail, the old size would include the tail.
      // Consequently, it has to be the old size. If we have no tail, we append
      // a single element to the old vector, therefore it has to be one more
      // than the original.

      new_root->size_table = table;
    }

    // nodes visited == original rrb tree height. Nodes visited > 0.
    PersistentVectorInternalNode **to_set = PersistentVectorInternalNode_appendEmpty(&((PersistentVectorInternalNode *) new_rrb->root)->child[1], nodes_visited);
    *to_set = (PersistentVectorInternalNode *) old_tail;
  }
  else {
    PersistentVectorInternalNode **node = PersistentVectorInternalNode_copyFirstK(rrb, new_rrb, nodes_to_copy, old_tail->len);
    PersistentVectorInternalNode **to_set = PersistentVectorInternalNode_appendEmpty(node, nodes_visited - nodes_to_copy);
    *to_set = (PersistentVectorInternalNode *) old_tail;
  }

  return new_rrb;
}


/* Node */

bool PersistentVectorNode_equals(PersistentVectorNode *self, PersistentVectorNode *other) {
  return false;
}

uint64_t PersistentVectorNode_hash(PersistentVectorNode *self) {
  return 0;
}

String *PersistentVectorNode_toString(PersistentVectorNode *self) {
  return NULL;
}

void PersistentVectorNode_destroy(PersistentVectorNode *self, bool deallocateChildren) {
  PersistentVectorLeafNode *leaf = (PersistentVectorLeafNode *)self;
  PersistentVectorInternalNode *internal = (PersistentVectorInternalNode *)self;
  switch (self->type) {
    case LEAF_NODE:
      for(int i=0; i<leaf->len; i++) Object_release(leaf->child[i]);
      break;
    case INTERNAL_NODE:
      for(int i=0; i<internal->len; i++) release(internal->child[i]);
      if (internal->size_table) deallocate(internal->size_table);
      break;
  }
}






