#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include "Object.hpp"
#include "PersistentList.hpp"
#include "Number.hpp"

using namespace std;

void testList () {
  auto l = new PersistentList<Object>();
  // l = l->conj(new Number(3));
  // l = l->conj(new Number(7));
  // l = l->conj(new PersistentList(new Number(2)));
  // l = l->conj(new PersistentList());
  string str;
  cout << "Press a key to start" << endl;
  getline(cin, str); 
  auto as = chrono::high_resolution_clock::now();

  for (int i=0;i<100000000; i++) {
    auto n = new Number(i);
   auto k = l->conj(n);
   n->release();
   l->release();
   
   l = k;
  }
  auto ap = chrono::high_resolution_clock::now();
  cout << "Array size: " << l->count << endl;
  cout << "Ref count: " << l->refCount << endl;
  cout << "Time: " << chrono::duration_cast<chrono::milliseconds>(ap-as).count() << endl;
   
  getline(cin, str); 
  auto os = chrono::high_resolution_clock::now();

  auto tmp = l;
  int64_t sum = 0;
  while(tmp != nullptr) {
    if(tmp->first) sum += dynamic_cast<Number *>(tmp->first)->value;
    tmp = tmp->rest;
  }

  cout << sum << endl;
  auto op = chrono::high_resolution_clock::now();
  cout << "Time: " <<  chrono::duration_cast<chrono::milliseconds>(op-os).count() << endl;
  getline(cin, str); 
  auto ds = chrono::high_resolution_clock::now();
  l->release();
  cout << "Released" << endl;
  cout << l->refCount << endl;
  auto dp = chrono::high_resolution_clock::now();
  cout << "Time: " <<  chrono::duration_cast<chrono::milliseconds>(dp-ds).count() << endl;

  getline(cin, str); 

  

}


int main() {
  testList();

}

