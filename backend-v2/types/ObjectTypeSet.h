#ifndef RT_OBJECT_TYPE_SET
#define RT_OBJECT_TYPE_SET

#include <set>
#include <vector>
#include <algorithm>
#include "../runtime/defines.h"
#include "../runtime/ObjectProto.h"

#include <string>
#include <iostream>
#include <sstream>
#include <cassert>

#include "ConstantBigInteger.h"
#include "ConstantInteger.h"
#include "ConstantBool.h"
#include "ConstantDouble.h"
#include "ConstantFunction.h"
#include "ConstantKeyword.h"
#include "ConstantNil.h"
#include "ConstantRatio.h"
#include "ConstantString.h"
#include "ConstantSymbol.h"
#include "ConstantClass.h"

namespace rt {

class ObjectTypeSet {
  private:
  std::set<objectType> internal;
  ObjectTypeConstant *constant = nullptr;
  bool isBoxed;
  public:

  ObjectTypeConstant * getConstant() const {
    return constant;
  }

  ObjectTypeSet(objectType type, bool isBoxed = false, ObjectTypeConstant *cons = nullptr) : constant(cons), isBoxed(isBoxed) {
    internal.insert(type);
    if(!constant && type == nilType) { 
      constant = static_cast<ObjectTypeConstant *>(new ConstantNil()); 
    } 
  }
  
  ObjectTypeSet() {}
  ~ObjectTypeSet() {
    if(constant) delete constant;
    constant = nullptr;
  }

  ObjectTypeSet(const ObjectTypeSet &other) : internal(other.internal), isBoxed(other.isBoxed) {
    if(other.constant) constant = other.constant->copy();
    else constant = nullptr;
  }
  
  std::set<objectType>::const_iterator internalBegin() const {
    return internal.begin();
  }
  
  std::set<objectType>::const_iterator internalEnd() const {
    return internal.end();
  }
      
  void insert(objectType type) {
    internal.emplace(type);
  }
    
  bool isEmpty() const {
    return internal.size() == 0;
  }

  int size() const {
    return internal.size();
  }

  bool contains(objectType type) const {
    return internal.find(type) != internal.end();
  }

  bool isBoxedType(objectType type) const {
    return contains(type) && internal.size() == 1 && isBoxed;
  }

  bool isUnboxedType(objectType type) const {
    return contains(type) && internal.size() == 1 && !isBoxed;
  }

  bool isDetermined() const {
    return internal.size() == 1;
  }
  
  bool isDynamic() const {
    if(internal.size() > 1) {
      return true;
    }
    return false;
  }
  
  bool isBoxedType() const {
    return isBoxed;
  }

  ObjectTypeSet unboxed() const {
    ObjectTypeSet retVal = *this;
    if(retVal.isDetermined()) retVal.isBoxed = false;
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
    if(isBoxed) return false;
    if(!isDetermined()) return false;
    switch(determinedType()) {
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
    if(!isBoxed) return false;
    if(!isDetermined()) return false;
    switch(determinedType()) {
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

  objectType determinedType() const {
    assert(isDetermined() && "Type not determined");
    return *(internal.begin());
  }

  void remove(objectType type) {
    auto pos = internal.find(type);
    if (pos == internal.end()) return;
    internal.erase(pos);
  }
    
  ObjectTypeSet expansion(const ObjectTypeSet &other) const {
    /* Expansion removes all constants */
    auto retVal = ObjectTypeSet(*this);
    retVal.internal.insert(other.internal.begin(), other.internal.end());
    retVal.isBoxed = (isBoxed && !isEmpty()) || (other.isBoxed && !other.isEmpty()) || retVal.size() > 1;
    if(retVal.constant) delete retVal.constant;
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
    /* Restriction preserves constant type for this */
    auto retVal = ObjectTypeSet();
    std::set_intersection(internal.begin(), internal.end(),
                          other.internal.begin(), other.internal.end(),                  
                          std::inserter(retVal.internal, retVal.internal.begin()));
    retVal.isBoxed = isBoxed;
    if (constant != nullptr && retVal.contains(constant->constantType)) {
      retVal.constant = constant->copy();
    } else {
      retVal.constant = nullptr;
    }
    return retVal;
  }

  ObjectTypeSet& operator=(const ObjectTypeSet &other) {
    isBoxed = other.isBoxed;
    internal = other.internal;
    if(constant) delete constant;
    constant = nullptr;
    if(other.constant) constant = other.constant->copy();
    return *this;
  }

  friend bool operator==(const ObjectTypeSet &first, const ObjectTypeSet &second);
  friend bool operator<(const ObjectTypeSet &first, const ObjectTypeSet &second);

  static ObjectTypeSet dynamicType() {
    auto all = ObjectTypeSet::all();
    all.isBoxed = true;
    return all;
  }
  
  static ObjectTypeSet empty() {
    return ObjectTypeSet();
  }

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
    retVal.isBoxed = true;
    return retVal;
  }
  
  static ObjectTypeSet fromVector(std::vector<objectType> types) {
    assert((types.size() > 1) && "ObjectTypeSet::fromVector requires vector of length at least 2");
    ObjectTypeSet retVal;
    for (auto t: types) retVal.insert(t);
    retVal.isBoxed = true;
    return retVal;
  }
  
  static std::string typeStringForArg(const ObjectTypeSet &arg) {
    if(!arg.isDetermined()) return "LO";
    else switch (arg.determinedType()) {
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
      }
  }
  
  static std::string typeStringForArgs(const std::vector<ObjectTypeSet> &args) {
    std::stringstream retval;
    for (auto i: args) retval << typeStringForArg(i);    
    return retval.str();
  }

  static std::string fullyQualifiedMethodKey(const std::string &name, const std::vector<ObjectTypeSet> &args, const ObjectTypeSet &retVal) {
    return name + "_" + typeStringForArgs(args) + "_" + typeStringForArg(retVal);
  }

  static std::string recursiveMethodKey(const std::string &name, const std::vector<ObjectTypeSet> &args) {
    return name + "_" + typeStringForArgs(args);
  }  

  std::string toString() const {
    std::string retVal = typeStringForArg(*this);    
    if(constant) retVal += " constant value: " + constant->toString() + " of " + std::to_string(constant->constantType);
    return retVal;
  }
};

} // namespace rt

#endif
