#ifndef RT_OBJECT_TYPE_SET
#define RT_OBJECT_TYPE_SET

#include <set>
#include <vector>
#include <algorithm>
#include "runtime/defines.h"

#include <string>
#include <iostream>
#include <sstream>
#include <cassert>
#include <gmp.h>

class ObjectTypeConstant {
  protected:
  ObjectTypeConstant(objectType type) : constantType(type) {}
  public:
  objectType constantType;
  virtual ~ObjectTypeConstant() {}
  virtual ObjectTypeConstant *copy() { return nullptr; }
  virtual std::string toString() { return ""; }
  virtual bool equals(ObjectTypeConstant *other) {return false; }
};

class ConstantInteger: public ObjectTypeConstant {
  public:
  int64_t value;
  ConstantInteger(int64_t val) : ObjectTypeConstant(integerType), value(val) {}
  virtual ObjectTypeConstant *copy() { return static_cast<ObjectTypeConstant *> (new ConstantInteger(value)); }
  virtual std::string toString() { return std::to_string(value); }
  virtual bool equals(ObjectTypeConstant *other) {   
    if(ConstantInteger *i = dynamic_cast<ConstantInteger *>(other)) {
      return i->value == value;
    }
    return false;
  }
};

class ConstantNil: public ObjectTypeConstant {
  public:
  ConstantNil() : ObjectTypeConstant(nilType) {}
  virtual ObjectTypeConstant *copy() { return static_cast<ObjectTypeConstant *> (new ConstantNil()); }
  virtual std::string toString() { return "nil"; }
  virtual bool equals(ObjectTypeConstant *other) {   
    return dynamic_cast<ConstantNil *>(other);
  }
};

class ConstantDouble: public ObjectTypeConstant {
  public:
  double value;
  ConstantDouble(double val) : ObjectTypeConstant(doubleType), value(val) {}
  virtual ObjectTypeConstant *copy() { return static_cast<ObjectTypeConstant *> (new ConstantDouble(value)); }
  virtual std::string toString() { return std::to_string(value); }
  virtual bool equals(ObjectTypeConstant *other) {   
    if(ConstantDouble *i = dynamic_cast<ConstantDouble *>(other)) {
      return i->value == value;
    }
    return false;
  }
};

class ConstantString: public ObjectTypeConstant {
  public:
  std::string value;
  ConstantString(std::string val) : ObjectTypeConstant(stringType), value(val) {}
  virtual ObjectTypeConstant *copy() { return static_cast<ObjectTypeConstant *> (new ConstantString(value)); }
  virtual std::string toString() { return value; }
  virtual bool equals(ObjectTypeConstant *other) {   
    if(ConstantString *i = dynamic_cast<ConstantString *>(other)) {
      return i->value == value;
    }
    return false;
  }
};

class ConstantBoolean: public ObjectTypeConstant {
  public:
  bool value;
  ConstantBoolean(bool val) : ObjectTypeConstant(booleanType), value(val) {}
  virtual ObjectTypeConstant *copy() { return static_cast<ObjectTypeConstant *> (new ConstantBoolean(value)); }
  virtual std::string toString() { return std::to_string(value); }
  virtual bool equals(ObjectTypeConstant *other) {   
    if(ConstantBoolean *i = dynamic_cast<ConstantBoolean *>(other)) {
      return i->value == value;
    }
    return false;
  }
};

class ConstantFunction: public ObjectTypeConstant {
  public:
  uint64_t value;
  ConstantFunction(uint64_t val) : ObjectTypeConstant(functionType), value(val) {}
  virtual ObjectTypeConstant *copy() { return static_cast<ObjectTypeConstant *> (new ConstantFunction(value)); }
  virtual std::string toString() { return std::string("fn_") + std::to_string(value); }
  virtual bool equals(ObjectTypeConstant *other) {
    if(ConstantFunction *i = dynamic_cast<ConstantFunction *>(other)) {
      return i->value == value;
    }
    return false;
  }
};

class ConstantKeyword: public ObjectTypeConstant {
  public:
  std::string value;
  ConstantKeyword(std::string val) : ObjectTypeConstant(keywordType), value(val) {}
  virtual ObjectTypeConstant *copy() { return static_cast<ObjectTypeConstant *> (new ConstantKeyword(value)); }
  virtual std::string toString() { return value; }
  virtual bool equals(ObjectTypeConstant *other) {   
    if(ConstantKeyword *i = dynamic_cast<ConstantKeyword *>(other)) {
      return i->value == value;
    }
    return false;
  }
};

class ConstantSymbol: public ObjectTypeConstant {
  public:
  std::string value;
  ConstantSymbol(std::string val) : ObjectTypeConstant(symbolType), value(val) {}
  virtual ObjectTypeConstant *copy() { return static_cast<ObjectTypeConstant *> (new ConstantSymbol(value)); }
  virtual std::string toString() { return value; }
  virtual bool equals(ObjectTypeConstant *other) {   
    if(ConstantSymbol *i = dynamic_cast<ConstantSymbol *>(other)) {
      return i->value == value;
    }
    return false;
  }
};

class ConstantBigInteger: public ObjectTypeConstant {
  public:
  mpz_t value;
  ConstantBigInteger(mpz_t val) : ObjectTypeConstant(bigIntegerType) { mpz_init_set(value, val); }
  ConstantBigInteger(std::string val) : ObjectTypeConstant(bigIntegerType) {
    assert(mpz_init_set_str(value, val.c_str(), 10) == 0 && "Failed to initialize BigInteger");
  }
  virtual ObjectTypeConstant *copy() { return static_cast<ObjectTypeConstant *> (new ConstantBigInteger(value)); }
  virtual std::string toString() { return std::string(mpz_get_str(NULL, 10, value)); }
  virtual bool equals(ObjectTypeConstant *other) {   
    if(ConstantBigInteger *i = dynamic_cast<ConstantBigInteger *>(other)) {
      return mpz_cmp(i->value, value) == 0;
    }
    return false;
  }
};


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
    if(!isScalar()) isBoxed = true;
  }
  ObjectTypeSet() {
  }
  ~ObjectTypeSet() {
    if(constant) delete constant;
    constant = nullptr;
  }

  ObjectTypeSet(const ObjectTypeSet &other) : internal(other.internal), isBoxed(other.isBoxed) {
    if(other.constant) constant = other.constant->copy();
    else constant = nullptr;
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
      assert(isBoxed == true && "Internal error");
      return true;
    }
    return false;
  }

  ObjectTypeSet unboxed() const {
    ObjectTypeSet retVal = *this;
    if(retVal.isBoxedScalar()) retVal.isBoxed = false;
    return retVal;
  }

  ObjectTypeSet boxed() const {
    ObjectTypeSet retVal = *this;
    retVal.isBoxed = true;
    return retVal;
  }



  bool isScalar() const {
    if(isBoxed) return false;
    if(!isDetermined()) return false;
    switch(determinedType()) {
      case integerType:
      case booleanType:
      case doubleType:
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
    retVal.isBoxed = isBoxed || other.isBoxed || retVal.size() > 1;
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
    auto all = ObjectTypeSet:: all();
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
    retVal.insert(persistentArrayMapType);
    retVal.isBoxed = true;
    return retVal;
  }
  
  static std::vector<ObjectTypeSet> allGuesses() {
    auto allTypes = ObjectTypeSet::all();
    std::vector<ObjectTypeSet> guesses;
    guesses.push_back(ObjectTypeSet(booleanType));
    guesses.push_back(ObjectTypeSet(integerType));
    guesses.push_back(ObjectTypeSet(doubleType));
    for(auto it: allTypes.internal) {
      guesses.push_back(ObjectTypeSet(it));
    }
    guesses.push_back(allTypes);
    return guesses;
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
        return (arg.isBoxed ? "LB" : "B");
      case nilType:
        return "LN";
      case keywordType:
        return "LK";
      case functionType:
        return "LF";
      case bigIntegerType:
        return "LI";
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


#endif
