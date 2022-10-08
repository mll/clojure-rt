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

class PersistentList: public Object {
  public:
  Object *first;
  PersistentList *rest;
  uint64_t count;
 
  PersistentList() : first(nullptr), rest(nullptr), count(0), Object(persistentListType) {}
  PersistentList(Object *object, PersistentList *remaining);
  PersistentList(Object *object);
  virtual ~PersistentList();

  PersistentList *conj(Object *object);
  bool equals(PersistentList *other);

  String *toString() override;
  uint64_t hash() override;
  bool equals(Object *other) override;
};


#endif
