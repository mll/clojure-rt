#include "ClassExtension.h"
#include "../tools/EdnParser.h"
#include "Exceptions.h"
#include <string>

using namespace rt;

extern "C" {
int32_t ClassExtension_fieldIndex(void *ext, RTValue field) {
  ClassDescription *description = (ClassDescription *)ext;
  String *s = String_compactify(::toString(field));
  std::string key = String_c_str(s);
  Ptr_release(s);

  auto it = description->instanceFields.find(key);
  if (it != description->instanceFields.end()) {
    return it->second;
  }
  return -1;
}

int32_t ClassExtension_staticFieldIndex(void *ext, RTValue staticField) {
  ClassDescription *description = (ClassDescription *)ext;
  String *s = String_compactify(::toString(staticField));
  std::string key = String_c_str(s);
  Ptr_release(s);

  auto it = description->staticFieldIndices.find(key);
  if (it != description->staticFieldIndices.end()) {
    return it->second;
  }
  return -1;
}

RTValue ClassExtension_getIndexedStaticField(void *ext, int32_t i) {
  ClassDescription *description = (ClassDescription *)ext;
  if (i >= 0 && i < (int32_t)description->staticFieldValues.size()) {
    RTValue val = description->staticFieldValues[i];
    retain(val);
    return val;
  }
  return RT_boxNil();
}

RTValue ClassExtension_setIndexedStaticField(void *ext, int32_t i,
                                             RTValue value) {
  ClassDescription *description = (ClassDescription *)ext;
  if (i >= 0 && i < (int32_t)description->staticFieldValues.size()) {
    RTValue old = description->staticFieldValues[i];
    description->staticFieldValues[i] = value;
    release(old);
    return value;
  }
  return value;
}

ClojureFunction *ClassExtension_resolveInstanceCall(void *ext, RTValue name,
                                                    int32_t argCount) {
  ClassDescription *description = (ClassDescription *)ext;
  String *s = String_compactify(::toString(name));
  std::string key = String_c_str(s);
  Ptr_release(s);

  auto it = description->instanceFns.find(key);
  if (it != description->instanceFns.end()) {
     // Currently returns nullptr as in the original implementation
  }
  return nullptr;
}
}
