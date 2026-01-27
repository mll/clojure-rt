#include "PersistentVectorIterator.h"
#include "PersistentVector.h"
#include "PersistentVectorNode.h"
#include "RTValue.h"

/* outside refcount system */
PersistentVectorIterator PersistentVector_iterator(PersistentVector *self) {
  PersistentVectorIterator it;
  it.index = 0;
  it.blockIndex = 0;
  if(self->count > 0) it.block = PersistentVector_nthBlock(self, 0);
  else it.block = NULL;
  it.parent = self;
  return it;
}

/* outside refcount system */
RTValue PersistentVector_iteratorGet(PersistentVectorIterator *it) {
  /* no bounds check, has to always be used carefully */
  return it->block->array[it->blockIndex];
}

/* outside refcount system */
RTValue PersistentVector_iteratorNext(PersistentVectorIterator *it) {
  it->blockIndex++;
  it->index++;
  if(it->blockIndex < it->block->count) {
    return it->block->array[it->blockIndex];
  } 
  if(it->parent->count == it->index) return RT_boxNull();
  PersistentVectorNode *newBlock = PersistentVector_nthBlock(it->parent, it->index);
  if(newBlock == NULL) return RT_boxNull();
  it->blockIndex = 0;
  it->block = newBlock;
  return it->block->array[0];
}
