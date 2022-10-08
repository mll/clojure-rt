#include <memory>
#include <string>
#include "Object.hpp"

using namespace std;



Object::Object(objectType type) : type(type), refCount(1) {
  
}


