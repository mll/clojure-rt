#ifndef RT_OBJECT_TYPE_SET
#define RT_OBJECT_TYPE_SET

#include <set>
#include <vector>
#include <algorithm>
#include "runtime/defines.h"

#include <string>

using namespace std;

class ObjectTypeSet {
  private:
  set<objectType> internal;
  public:
  ObjectTypeSet(objectType type) {
    internal.insert(type);
  }
  ObjectTypeSet() {}
  ObjectTypeSet(const ObjectTypeSet &other) : internal(other.internal) {}
    
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
  
  ObjectTypeSet setUnion(const ObjectTypeSet &other) const {
    auto retVal = ObjectTypeSet(*this);
    retVal.internal.insert(other.internal.begin(), other.internal.end());
    return retVal;
  }

  ObjectTypeSet difference(const ObjectTypeSet &other) const {
    auto retVal = ObjectTypeSet();
    std::set_difference(internal.begin(), internal.end(),
                        other.internal.begin(), other.internal.end(),                  
                        std::inserter(retVal.internal, retVal.internal.begin()));
    return retVal;
  }

  ObjectTypeSet intersection(const ObjectTypeSet &other) const {
    auto retVal = ObjectTypeSet();
    std::set_intersection(internal.begin(), internal.end(),
                   other.internal.begin(), other.internal.end(),                  
                   std::inserter(retVal.internal, retVal.internal.begin()));
    return retVal;
  }

  string toString() const {
    vector<objectType> types(internal.begin(), internal.end());
    string ss;
    
    for (int i=0; i < types.size(); i++) {
      ss += to_string((int)types[i]);
      if(i < types.size() - 1) ss += string(",");
    }
    return ss;
  }

  friend bool operator==(const ObjectTypeSet &first, const ObjectTypeSet &second);
  
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

};


#endif
