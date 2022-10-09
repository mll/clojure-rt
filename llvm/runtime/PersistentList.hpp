#ifndef RT_PERSISTENT_LIST
#define RT_PERSISTENT_LIST

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include <list>
#include "Object.hpp"

using namespace std;

template <class T>
class PersistentList: public Object {
  public:
  T *first;
  PersistentList<T> *rest;
  uint64_t count;
 
  PersistentList() : first(nullptr), rest(nullptr), count(0), Object(persistentListType) {}
  PersistentList(T *object, PersistentList<T> *remaining);
  PersistentList(T *object);
  virtual ~PersistentList();

  PersistentList<T> *conj(T *object);
  bool equals(PersistentList<T> *other);

  String *toString() override;
  uint64_t hash() override;
  bool equals(Object *other) override;
};

template <class T>
PersistentList<T>::PersistentList(T *object, PersistentList<T> *remaining) : first(object), Object(persistentListType), rest(remaining), count(1 + rest->count) {
  object->retain();
  remaining->retain();
}

template <class T>
PersistentList<T>::PersistentList(T *object) : first(object), Object(persistentListType), rest(nullptr), count(1) {
  object->retain();
}


template <class T>
PersistentList<T>::~PersistentList() {
  if (shouldDeallocateChildren) {
    auto child = rest;
    while(child != nullptr) {
      auto next = child->rest;
      if (!child->release(false)) break;
      child = next;
    }
  }
  if(first) first->release();
}

template <class T>
PersistentList<T> *PersistentList<T>::conj(T *object) {
  return new PersistentList<T>(object, this);
}

template <class T>
bool PersistentList<T>::equals(PersistentList<T> *other) {
  if (this == other) return true;
  if (this->count != other->count) return false;
  bool firstEqual = first == other->first || (first && other->first && first->equals(other->first));
  bool restEqual = rest == other->rest || (rest && other->rest && rest->equals(other->rest));
  return firstEqual && restEqual;
}

template <class T>
String *PersistentList<T>::toString() {
  string result("(");
  PersistentList<T> *current = this;
  while (current) {
    if (current != this && current->first) result.append(" "); 
    if (current->first) {
      auto s = current->first->toString();
      result.append(s->value);
      s->release();
    }
    current = current->rest;
  }
  result.append(")");
  return new String(result);
}

template <class T>
uint64_t PersistentList<T>::hash() {
  return (first != nullptr ? first->hash() : 0) + (rest != nullptr ? rest->hash() : 0);
}

template <class T>
bool PersistentList<T>::equals(Object *other) { 
  if (this == other) return true;
  if (other->type != persistentListType) return false;
  auto typedOther = dynamic_cast<PersistentList *>(other);
  return equals(typedOther);
}

#endif
