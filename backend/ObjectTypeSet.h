#ifndef RT_OBJECT_TYPE_SET
#define RT_OBJECT_TYPE_SET

#include <set>
#include <vector>
#include <algorithm>
#include "runtime/defines.h"
#include "runtime/ObjectProto.h"

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
  ConstantBigInteger(mpz_t val) : ObjectTypeConstant(bigIntegerType) {
    mpz_init_set(value, val);
  }
  ConstantBigInteger(mpq_t val) : ObjectTypeConstant(bigIntegerType) { 
    assert(mpz_cmp_si(mpq_denref(val), 1) == 0);
    mpz_init_set(value, mpq_numref(val));
  }
  ConstantBigInteger(std::string val) : ObjectTypeConstant(bigIntegerType) {
    assert(mpz_init_set_str(value, val.c_str(), 10) == 0 && "Failed to initialize BigInteger");
  }
  ~ConstantBigInteger() {
    mpz_clear(value);
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

class ConstantRatio: public ObjectTypeConstant {
  public:
  mpq_t value;
  ConstantRatio(mpq_t val) : ObjectTypeConstant(ratioType) { 
    mpq_init(value);
    mpq_set(value, val);
  }
  ConstantRatio(std::string val) : ObjectTypeConstant(ratioType) {
    mpq_init(value);
    assert(mpq_set_str(value, val.c_str(), 10) == 0 && "Failed to initialize Ratio");
    mpq_canonicalize(value);
  }
  ConstantRatio(int64_t num, int64_t den) : ObjectTypeConstant(ratioType) {
    mpq_init(value);
    mpq_set_si(value, num, den);
    mpq_canonicalize(value);
  }
  ConstantRatio(mpz_t num, mpz_t den) : ObjectTypeConstant(ratioType) {
    mpq_init(value);
    mpq_set_z(value, num);
    mpz_clear(num);
    mpq_t den2;
    mpq_init(den2);
    mpq_set_z(den2, den);
    mpz_clear(den);
    mpq_div(value, value, den2);
    mpq_clear(den2);
  }
  ~ConstantRatio() {
    mpq_clear(value);
  }
  virtual ObjectTypeConstant *copy() { return static_cast<ObjectTypeConstant *> (new ConstantRatio(value)); }
  virtual std::string toString() { return std::string(mpq_get_str(NULL, 10, value)); }
  virtual bool equals(ObjectTypeConstant *other) {   
    if(ConstantRatio *i = dynamic_cast<ConstantRatio *>(other)) {
      return mpq_equal(i->value, value);
    }
    return false;
  }
};

class ObjectTypeSet {
  private:
  std::set<objectType> internal;
  ObjectTypeConstant *constant = nullptr;
  inline static const uint64_t ANY = 0;
  // internalClasses might contain mysterious value ANY, which means, that it can be any class
  // set might contain either normal class names or one value ANY
  // internalClasses is nonempty iff internal contains deftypeType
  std::set<uint64_t> internalClasses;
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
    if(!isScalar()) this->isBoxed = true;
    if (type == deftypeType) internalClasses.insert(ANY);
  }
  ObjectTypeSet(uint64_t classId) : constant(nullptr), isBoxed(true) {
    internal.insert(deftypeType);
    isBoxed = true;
    internalClasses.insert(classId);
  }
  ObjectTypeSet() {
  }
  ~ObjectTypeSet() {
    if(constant) delete constant;
    constant = nullptr;
  }

  ObjectTypeSet(const ObjectTypeSet &other) : internal(other.internal), internalClasses(other.internalClasses), isBoxed(other.isBoxed) {
    if(other.constant) constant = other.constant->copy();
    else constant = nullptr;
  }
  
  std::set<objectType>::const_iterator internalBegin() const {
    return internal.begin();
  }
  
  std::set<objectType>::const_iterator internalEnd() const {
    return internal.end();
  }
  
  std::set<uint64_t>::const_iterator internalClassesBegin() const {
    return internalClasses.begin();
  }
  
  std::set<uint64_t>::const_iterator internalClassesEnd() const {
    return internalClasses.end();
  }
    
  void insert(objectType type) {
    internal.emplace(type);
    if (type == deftypeType) internalClasses = {ANY};
  }
  
  bool anyClass() const {
    return internalClasses.find(ANY) != internalClasses.end();
  }
  
  void insertClass(uint64_t classId) {
    internal.emplace(deftypeType);
    if (!anyClass()) internalClasses.emplace(classId);
  }

  bool isEmpty() const {
    return internal.size() == 0;
  }

  int size() const {
    return internal.size();
  }

  int classesSize() const {
    return internalClasses.size();
  }

  bool contains(objectType type) const {
    return internal.find(type) != internal.end();
  }

  bool containsClass(uint64_t classId) const {
    return contains(deftypeType) && (anyClass() || (internalClasses.find(classId) != internalClasses.end()));
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
  
  bool isDeterminedClass() const {
    return internal.size() == 1 && determinedType() == deftypeType && internalClasses.size() == 1 && !anyClass();
  }

  bool isDynamic() const {
    if(internal.size() > 1) {
      assert(isBoxed == true && "Internal error");
      return true;
    }
    return contains(deftypeType);
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

  uint64_t determinedClass() const {
    assert(isDeterminedClass() && "Class not determined");
    return *(internalClasses.begin());
  }

  void remove(objectType type) {
    auto pos = internal.find(type);
    if (pos == internal.end()) return;
    internal.erase(pos);
    if (type == deftypeType) internalClasses.clear();
  }
  
  void removeClass(uint64_t classId) {
    assert(classId != ANY);
    auto pos = internalClasses.find(classId);
    if (pos == internalClasses.end()) return;
    internalClasses.erase(pos);
    if (internalClasses.empty()) remove(deftypeType);
  }
  
  ObjectTypeSet expansion(const ObjectTypeSet &other) const {
    /* Expansion removes all constants */
    auto retVal = ObjectTypeSet(*this);
    retVal.internal.insert(other.internal.begin(), other.internal.end());
    if (other.anyClass() || retVal.anyClass()) {
      retVal.internalClasses = {ANY};
    } else {
      retVal.internalClasses.insert(other.internalClasses.begin(), other.internalClasses.end());
    }
    retVal.isBoxed = (isBoxed && !isEmpty()) || (other.isBoxed && !other.isEmpty()) || retVal.size() > 1;
    if(retVal.constant) delete retVal.constant;
    retVal.constant = nullptr;
    return retVal;
  }

  ObjectTypeSet removeConst() const {
    /* Expansion removes all constants */
    auto retVal = ObjectTypeSet();
    retVal.internal = internal;
    retVal.internalClasses = internalClasses;
    retVal.isBoxed = isBoxed; 
    return retVal;
  }
  

  ObjectTypeSet restriction(const ObjectTypeSet &other) const {
    /* Restriction preserves constant type for this */
    auto retVal = ObjectTypeSet();
    std::set_intersection(internal.begin(), internal.end(),
                          other.internal.begin(), other.internal.end(),                  
                          std::inserter(retVal.internal, retVal.internal.begin()));
    if (!retVal.contains(deftypeType)) {
      retVal.internalClasses = {};
    } else if (anyClass()) {
      retVal.internalClasses = other.internalClasses;
    } else if (other.anyClass()) {
      retVal.internalClasses = internalClasses;
    } else {
      std::set_intersection(internalClasses.begin(), internalClasses.end(),
                            other.internalClasses.begin(), other.internalClasses.end(),                  
                            std::inserter(retVal.internalClasses, retVal.internalClasses.begin()));
    }
    retVal.isBoxed = isBoxed;
    if (constant != nullptr && retVal.contains(constant->constantType)) {
      retVal.constant = constant->copy();
    } else {
      retVal.constant = nullptr;
    }
    if (retVal.internalClasses.empty()) retVal.remove(deftypeType);
    return retVal;
  }

  ObjectTypeSet& operator=(const ObjectTypeSet &other) {
    isBoxed = other.isBoxed;
    internal = other.internal;
    internalClasses = other.internalClasses;
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
    retVal.insert(classType);
    retVal.insert(deftypeType);
    retVal.insert(keywordType);
    retVal.insert(functionType);
    retVal.insert(varType);
    retVal.insert(bigIntegerType);
    retVal.insert(ratioType);
    retVal.insert(persistentArrayMapType);
    retVal.internalClasses = {ANY};
    retVal.isBoxed = true;
    return retVal;
  }
  
  static std::vector<std::string> allTypes() {
    return {"J", "D", "Z", "LS", "LV", "LL", "LY", "LK", "LF", "LN", "LO", "LT", "LC", "LQ"};
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
      case classType:
        return "LC";
      case deftypeType:
        if (arg.isDeterminedClass()) return "C" + std::to_string(arg.determinedClass()) + ";";
        return "LT";
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
      case varType:
        return "LQ";
      case bigIntegerType:
        return "LI";
      case ratioType:
        return "LR";
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
