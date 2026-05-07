#ifndef TYPED_VALUE_H
#define TYPED_VALUE_H
#include "../types/ObjectTypeSet.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"

namespace rt {
struct TypedValue {
  ObjectTypeSet type;
  llvm::Value *value;
  TypedValue() : type(ObjectTypeSet::empty()), value(nullptr) {}
  TypedValue(const ObjectTypeSet &t, llvm::Value *v) : type(t), value(v) {}

  bool operator==(const TypedValue &other) const {
    return value == other.value && type == other.type;
  }

  bool operator<(const TypedValue &other) const {
    if (value != other.value)
      return value < other.value;
    return type < other.type;
  }
};
} // namespace rt

#endif
