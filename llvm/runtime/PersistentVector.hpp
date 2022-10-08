#ifndef RT_PERSISTENT_VECTOR
#define RT_PERSISTENT_VECTOR

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <iostream>
#include "Object.hpp"

using namespace std;

class PersistentVectorNode: public Object {
  public:
  unique_ptr<vector<Object *>> array;
  shared_ptr<mutex> edit;
  PersistentVectorNode(shared_ptr<mutex> edit) : edit(edit), array(make_unique<vector<Object *>>(32)) {}
  PersistentVectorNode(shared_ptr<mutex> edit, unique_ptr<vector<Object *>> array) : edit(edit), array(move(array)) {}
a}

class TransientVector: public Object {
  volatile uint64_t cnt;
  volatile uint64_t shift;
  volatile PersistentVectorNode *root;
  volatile unique_ptr<vector<Object *>> tail;

  TransientVector(int cnt, int shift, PersistentVectorNode *root, unique_ptr<vector<Object *>> tail) : cnt(cnt), shift(shift), root(root) tail(move(tail)) {
    // root->retain(); // memory management to be decided later
}

}






class PersistentVector: public Object {
  public:
  uint64_t cnt;
  uint64_t shift;
  PersistentVectorNode *root;
  unique_ptr<vector<Object *>> tail;
// _meta

  PersistentVector() : Object(persistentVectorType), count(0) {}
  virtual ~PersistentVector();

  PersistentVector *conj(Object *object);
  bool equals(PersistentVector *other);

  String *toString() override;
  uint64_t hash() override;
  bool equals(Object *other) override;
};

#endif
