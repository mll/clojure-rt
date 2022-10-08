#ifndef RT_OBJECT
#define RT_OBJECT

#include <memory>
#include <string>
#include <mutex>
#include <functional>

using namespace std;

enum objectType {
   numberType,
   stringType,
   persistentListType,
   persistentVector,
   unknownType
};

class String;

class Object {
  public:
  objectType type;

  mutex refCountLock;
  uint64_t refCount;
  bool shouldDeallocateChildren;
  inline void retain();
  inline bool release(bool deallocateChildren = true);

  Object(objectType type);
  virtual ~Object() {}

  virtual bool equals(Object *other) = 0;
  virtual uint64_t hash() = 0;
  virtual String *toString() = 0;
};

class String: public Object {
  public:
  string value;
  virtual ~String();
  String(string value);

  bool equals(String *other);
  uint64_t length();
  
  virtual String *toString() override;
  virtual uint64_t hash() override; 
  virtual bool equals(Object *other) override;
};


inline void Object::retain() {
  refCountLock.lock();
  refCount++;
  refCountLock.unlock();
}

inline bool Object::release(bool deallocateChildren) {
  refCountLock.lock();
  refCount--;
  if (refCount == 0) {
    shouldDeallocateChildren = deallocateChildren;
    delete this;
    return true;
  }
  else { 
    refCountLock.unlock();
    return false;
  }
}


#endif
