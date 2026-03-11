#ifndef TYPED_VALUE_H
#define TYPED_VALUE_H
#include "../types/ObjectTypeSet.h"
#include "llvm/IR/Type.h"

namespace rt {
struct TypedValue {
  ObjectTypeSet type;
  llvm::Value *value;
  TypedValue(const ObjectTypeSet &t, llvm::Value *v) : type(t), value(v) {}
};
} // namespace rt

#endif
