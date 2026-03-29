#ifndef RT_OBJECT_TYPE_SET
#define RT_OBJECT_TYPE_SET

#include "../runtime/ObjectProto.h"
#include "../runtime/defines.h"
#include <_types/_uint32_t.h>
#include <algorithm>
#include <set>
#include <vector>

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

#include "ConstantBigInteger.h"
#include "ConstantBool.h"
#include "ConstantClass.h"
#include "ConstantDouble.h"
#include "ConstantFunction.h"
#include "ConstantInteger.h"
#include "ConstantKeyword.h"
#include "ConstantNil.h"
#include "ConstantRatio.h"
#include "ConstantString.h"
#include "ConstantSymbol.h"
#include "bridge/Exceptions.h"

namespace rt {

class ObjectTypeSet {
private:
  std::set<uint32_t> internal;
  ObjectTypeConstant *constant = nullptr;
  bool isBoxed = false;
  bool allTypes = false;

public:
  ObjectTypeConstant *getConstant() const { return constant; }

  ObjectTypeSet(uint32_t type, bool isBoxed = false,
                ObjectTypeConstant *cons = nullptr)
      : constant(cons), isBoxed(isBoxed) {
    internal.insert(type);
    if (!constant && type == nilType) {
      constant = static_cast<ObjectTypeConstant *>(new ConstantNil());
    }
  }

  ObjectTypeSet() {}

  ~ObjectTypeSet() {
    if (constant)
      delete constant;
    constant = nullptr;
  }

  ObjectTypeSet(const ObjectTypeSet &other)
      : internal(other.internal), isBoxed(other.isBoxed) {
    if (other.constant)
      constant = other.constant->copy();
    else
      constant = nullptr;
  }

  void insert(uint32_t type) { internal.emplace(type); }

  bool isEmpty() const { return internal.size() == 0 && !allTypes; }

  int size() const { return internal.size(); }

  bool contains(uint32_t type) const {
    if (allTypes)
      return true;
    return internal.find(type) != internal.end();
  }

  bool isBoxedType(uint32_t type) const {
    return contains(type) && internal.size() == 1 && isBoxed && !allTypes;
  }

  bool isUnboxedType(uint32_t type) const {
    return contains(type) && internal.size() == 1 && !isBoxed && !allTypes;
  }

  bool isDetermined() const { return internal.size() == 1 && !allTypes; }

  bool isDynamic() const {
    if (internal.size() > 1 || allTypes) {
      return true;
    }
    return false;
  }

  bool isBoxedType() const { return isBoxed; }

  ObjectTypeSet unboxed() const {
    ObjectTypeSet retVal = *this;
    if (retVal.isDetermined())
      retVal.isBoxed = false;
    return retVal;
  }

  ObjectTypeSet boxed() const {
    ObjectTypeSet retVal = *this;
    retVal.isBoxed = true;
    return retVal;
  }

  bool isUnboxedPointer() const {
    return !isScalar() && !isBoxedScalar() && !isBoxed;
  }

  bool isBoxedPointer() const {
    return isDetermined() && !isScalar() && !isBoxedScalar() && isBoxed;
  }

  bool isScalar() const {
    if (isBoxed)
      return false;
    if (!isDetermined())
      return false;
    switch (determinedType()) {
    case integerType:
    case booleanType:
    case doubleType:
    case nilType:
    case keywordType:
    case symbolType:
      return true;
    default:
      return false;
    }
  }

  bool isBoxedScalar() const {
    if (!isBoxed)
      return false;
    if (!isDetermined())
      return false;
    switch (determinedType()) {
    case integerType:
    case booleanType:
    case doubleType:
    case nilType:
    case keywordType:
    case symbolType:
      return true;
    default:
      return false;
    }
  }

  uint32_t determinedType() const {
    if (!isDetermined()) {
      throwInternalInconsistencyException("Type not determined");
    }
    return *(internal.begin());
  }

  void remove(uint32_t type) {
    auto pos = internal.find(type);
    if (pos == internal.end())
      return;
    internal.erase(pos);
  }

  ObjectTypeSet expansion(const ObjectTypeSet &other) const {
    /* Expansion removes all constants */
    auto retVal = ObjectTypeSet(*this);
    if (other.allTypes) {
      retVal.allTypes = true;
      return retVal;
    }
    retVal.internal.insert(other.internal.begin(), other.internal.end());
    retVal.isBoxed = (isBoxed && !isEmpty()) ||
                     (other.isBoxed && !other.isEmpty()) || retVal.size() > 1;
    if (retVal.constant)
      delete retVal.constant;
    retVal.constant = nullptr;
    return retVal;
  }

  ObjectTypeSet removeConst() const {
    /* Expansion removes all constants */
    auto retVal = ObjectTypeSet();
    retVal.internal = internal;
    retVal.isBoxed = isBoxed;
    return retVal;
  }

  ObjectTypeSet restriction(const ObjectTypeSet &other) const {
    if (other.allTypes) {
      return *this;
    }
    /* Restriction preserves constant type for this */
    auto retVal = ObjectTypeSet();
    std::set_intersection(
        internal.begin(), internal.end(), other.internal.begin(),
        other.internal.end(),
        std::inserter(retVal.internal, retVal.internal.begin()));
    retVal.isBoxed = isBoxed;
    if (constant != nullptr && retVal.contains(constant->constantType)) {
      retVal.constant = constant->copy();
    } else {
      retVal.constant = nullptr;
    }
    return retVal;
  }

  ObjectTypeSet &operator=(const ObjectTypeSet &other) {
    if (this == &other)
      return *this;
    isBoxed = other.isBoxed;
    internal = other.internal;
    allTypes = other.allTypes;
    if (constant)
      delete constant;
    constant = nullptr;
    if (other.constant)
      constant = other.constant->copy();
    return *this;
  }

  friend bool operator==(const ObjectTypeSet &first,
                         const ObjectTypeSet &second);
  friend bool operator<(const ObjectTypeSet &first,
                        const ObjectTypeSet &second);

  static ObjectTypeSet dynamicType() {
    auto all = ObjectTypeSet::all();
    all.isBoxed = true;
    return all;
  }

  static ObjectTypeSet empty() { return ObjectTypeSet(); }

  static ObjectTypeSet all() {
    ObjectTypeSet retVal;
    retVal.insert(integerType);
    retVal.insert(stringType);
    retVal.insert(persistentListType);
    retVal.insert(persistentVectorType);
    retVal.insert(doubleType);
    retVal.insert(nilType);
    retVal.insert(booleanType);
    retVal.insert(symbolType);
    retVal.insert(keywordType);
    retVal.insert(functionType);
    retVal.insert(bigIntegerType);
    retVal.insert(ratioType);
    retVal.insert(classType);
    retVal.insert(persistentArrayMapType);
    retVal.insert(varType);
    retVal.allTypes = true;
    retVal.isBoxed = true;
    return retVal;
  }

  static ObjectTypeSet fromVector(std::vector<uint32_t> types) {
    assert((types.size() > 1) &&
           "ObjectTypeSet::fromVector requires vector of length at least 2");
    ObjectTypeSet retVal;
    for (auto t : types)
      retVal.insert(t);
    retVal.isBoxed = true;
    return retVal;
  }

  static std::string typeStringForArg(const ObjectTypeSet &arg) {
    if (!arg.isDetermined())
      return "LO";
    else
      switch (arg.determinedType()) {
      case integerType:
        return (arg.isBoxed ? "LJ" : "J");
      case stringType:
        return "LS";
      case persistentListType:
        return "LL";
      case persistentVectorType:
        return "LV";
      case symbolType:
        return "LY";
      case persistentVectorNodeType:
        assert(false && "Node cannot be used as an argument");
      case concurrentHashMapType:
        assert(false && "Concurrent hash map cannot be used as an argument");
      case doubleType:
        return (arg.isBoxed ? "LD" : "D");
      case booleanType:
        return (arg.isBoxed ? "LZ" : "Z");
      case nilType:
        return "LN";
      case keywordType:
        return "LK";
      case functionType:
        return "LF";
      case bigIntegerType:
        return "LI";
      case ratioType:
        return "LR";
      case classType:
        return "LC";
      case persistentArrayMapType:
        return "LA";
      case varType:
        return "LQ";
      default:
        return "LR";
      }
  }

  static std::string typeStringForArgs(const std::vector<ObjectTypeSet> &args) {
    std::stringstream retval;
    for (auto i : args)
      retval << typeStringForArg(i);
    return retval.str();
  }

  static std::string
  fullyQualifiedMethodKey(const std::string &name,
                          const std::vector<ObjectTypeSet> &args,
                          const ObjectTypeSet &retVal) {
    return name + "_" + typeStringForArgs(args) + "_" +
           typeStringForArg(retVal);
  }

  static std::string
  recursiveMethodKey(const std::string &name,
                     const std::vector<ObjectTypeSet> &args) {
    return name + "_" + typeStringForArgs(args);
  }

  static std::string toHumanReadableName(uint32_t type) {
    switch (type) {
    case integerType:
      return ":int";
    case stringType:
      return ":string";
    case persistentListType:
      return ":list";
    case persistentVectorType:
      return ":vector";
    case doubleType:
      return ":double";
    case nilType:
      return ":nil";
    case booleanType:
      return ":bool";
    case symbolType:
      return ":symbol";
    case keywordType:
      return ":keyword";
    case functionType:
      return ":fn";
    case bigIntegerType:
      return ":bigint";
    case ratioType:
      return ":ratio";
    case classType:
      return ":class";
    case persistentArrayMapType:
      return ":map";
    case persistentVectorNodeType:
      return ":vector-node";
    case concurrentHashMapType:
      return ":chm";
    case varType:
      return ":var";
    default:
      return ":custom(" + std::to_string(type) + ")";
    }
  }

  std::string toString() const {
    if (*this == ObjectTypeSet::all() ||
        *this == ObjectTypeSet::dynamicType()) {
      return ":any";
    }
    std::stringstream ss;
    if (isBoxed)
      ss << "^:b ";
    if (internal.empty()) {
      ss << ":empty";
    } else if (internal.size() == 1) {
      ss << toHumanReadableName(*internal.begin());
      if (constant) {
        ss << "=" << constant->toString();
      }
    } else {
      ss << "(";
      for (auto it = internal.begin(); it != internal.end(); ++it) {
        ss << toHumanReadableName(*it)
           << (std::next(it) == internal.end() ? "" : "|");
      }
      ss << ")";
    }
    return ss.str();
  }
};

} // namespace rt

#endif
