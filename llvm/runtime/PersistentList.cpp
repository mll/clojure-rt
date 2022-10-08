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
#include "PersistentList.hpp"

using namespace std;

/* Implementation */


PersistentList::PersistentList(Object *object, PersistentList *remaining) : first(object), Object(persistentListType), rest(remaining), count(1 + rest->count) {
  object->retain();
  remaining->retain();
}


PersistentList::PersistentList(Object *object) : first(object), Object(persistentListType), rest(nullptr), count(1) {
  object->retain();
}



PersistentList::~PersistentList() {
  if (shouldDeallocateChildren) {
    auto children = vector<Object *>();
    auto os = chrono::high_resolution_clock::now();
    for(auto child = rest; child != nullptr; child = child->rest) children.push_back(child);        
    auto op = chrono::high_resolution_clock::now();
    cout << "Children Time: " <<  chrono::duration_cast<chrono::milliseconds>(op-os).count() << endl;

    for(int i = 0; i < count - 1; i++) {
      if(!children[i]->release(false)) break;
    }
  }
  if(first) first->release();
}


PersistentList *PersistentList::conj(Object *object) {
  return new PersistentList(object, this);
}


bool PersistentList::equals(PersistentList *other) {
  if (this == other) return true;
  if (this->count != other->count) return false;
  bool firstEqual = first == other->first || (first && other->first && first->equals(other->first));
  bool restEqual = rest == other->rest || (rest && other->rest && rest->equals(other->rest));
  return firstEqual && restEqual;
}


String *PersistentList::toString() {
  string result("(");
  PersistentList *current = this;
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


uint64_t PersistentList::hash() {
  return (first != nullptr ? first->hash() : 0) + (rest != nullptr ? rest->hash() : 0);
}


bool PersistentList::equals(Object *other) { 
  if (this == other) return true;
  if (other->type != persistentListType) return false;
  auto typedOther = dynamic_cast<PersistentList *>(other);
  return equals(typedOther);
}
