#ifndef RT_OBJECT_TYPE_SET
#define RT_OBJECT_TYPE_SET

#include <set>
#include <vector>
#include <algorithm>
#include "runtime/defines.h"

#include <string>
#include <iostream>


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
  uint64_t value;
  ConstantInteger(uint64_t val) : ObjectTypeConstant(integerType), value(val) {}
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


class ObjectTypeSet {
  private:
  std::set<objectType> internal;
  ObjectTypeConstant *constant = nullptr;
  public:
  bool isBoxed;
  
  ObjectTypeConstant * getConstant() const {
    return constant;
  }

  ObjectTypeSet(objectType type, bool isBoxed = false, ObjectTypeConstant *cons = nullptr) : constant(cons), isBoxed(isBoxed) {
    internal.insert(type);
    if(!constant && type == nilType) { 
      constant = static_cast<ObjectTypeConstant *>(new ConstantNil()); 
    } 
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

  bool isEmpty() {
    return internal.size() == 0;
  }

  int size() {
    return internal.size();
  }

  bool contains(objectType type) const {
    return internal.find(type) != internal.end();
  }

  bool isType(objectType type) const {
    return contains(type) && internal.size() == 1;
  }

  bool isDetermined() const {
    return internal.size() == 1;
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

  bool isScalar() const {
    if(isBoxed || !isDetermined()) return false;
    objectType type = determinedType();
    if(type == integerType || type == doubleType || type == booleanType) return true;
    return false;
  }


  ObjectTypeSet& operator=(const ObjectTypeSet &other) {
    isBoxed = other.isBoxed;
    internal = other.internal;
    if(constant) delete constant;
    constant = nullptr;
    if(other.constant) constant = other.constant->copy();
    return *this;
  }

  std::string toString() const {
    std::vector<objectType> types(internal.begin(), internal.end());
    std::string ss;
    
    for (int i=0; i < types.size(); i++) {
      ss += std::to_string((int)types[i]);
      if(i < types.size() - 1) ss += std::string(",");
    }
    
    if(constant) ss += " constant value: " + constant->toString();
    if(isBoxed) ss += " BOXED";
    else ss += " UNBOXED";
    return ss;
  }

  friend bool operator==(const ObjectTypeSet &first, const ObjectTypeSet &second);

  static ObjectTypeSet dynamicType() {
    auto all = ObjectTypeSet:: all();
    all.isBoxed = true;
    return all;
  }
  
  static ObjectTypeSet all() {
    ObjectTypeSet retVal;
    retVal.insert(integerType);
    retVal.insert(stringType);
    retVal.insert(persistentListType);
    retVal.insert(persistentVectorType);
    retVal.insert(persistentVectorNodeType);
    retVal.insert(doubleType);
    retVal.insert(nilType);
    retVal.insert(booleanType);
    retVal.insert(symbolType);
    retVal.insert(keywordType);
    retVal.insert(concurrentHashMapType);
    retVal.insert(functionType);
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
};


#endif
