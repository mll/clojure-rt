#include <string>
#include <vector>

extern "C" {
  #include "runtime/Class.h"
  #include "runtime/String.h"
  void retain(void *obj);
  void *allocate(size_t size);
  typedef struct Integer Integer;
  Integer* Integer_create(int64_t integer);
  Object *super(void *obj);
}

Class *javaLangClass() {
    const char *s = "class java.lang.Class";
    String *name = String_createDynamicStr(s);
    retain(name);
    return Class_create(name, name, 0, NULL, NULL, 0);
}

Class *javaLangLong() {
    const char *s = "class java.lang.Long";
    String *name = String_createDynamicStr(s);
    retain(name);
    std::vector<std::string> fieldNames {"BYTES", "MAX_VALUE", "MIN_VALUE", "SIZE" /*, "TYPE"*/};
    Keyword **staticFieldNames = (Keyword **) allocate(fieldNames.size() * sizeof(Keyword *));
    for (size_t i = 0; i < fieldNames.size(); ++i) staticFieldNames[i] = Keyword_create(String_createDynamicStr(fieldNames[i].c_str()));
    Object **staticFields = (Object **) allocate(fieldNames.size() * sizeof(Object *));
    staticFields[0] = super(Integer_create(8));
    staticFields[1] = super(Integer_create(INT64_MAX));
    staticFields[2] = super(Integer_create(INT64_MIN));
    staticFields[3] = super(Integer_create(64));
    // staticFields[4] = ???
    return Class_create(name, name, fieldNames.size(), staticFieldNames, staticFields, 0);
}
