#include <string>
#include <vector>
#include <numeric>

extern "C" {
  #include "runtime/Class.h"
  #include "runtime/String.h"
  Class *UNIQUE_UnboundClass;
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

Class *clojureAsmOpcodes() {
  const char *s = "class clojure.asm.Opcodes";
  String *name = String_createDynamicStr(s);
  retain(name);
  std::vector<std::vector<std::string>> flagNames {
    {"ACC_PUBLIC"}, // 0x0001
    {"ACC_PRIVATE"}, // 0x0002
    {"ACC_PROTECTED"}, // 0x0004 etc.
    {"ACC_STATIC"},
    {"ACC_FINAL"},
    {"ACC_SUPER", "ACC_SYNCHRONIZED", "ACC_OPEN", "ACC_TRANSITIVE"},
    {"ACC_VOLATILE", "ACC_BRIDGE", "ACC_STATIC_PHASE"},
    {"ACC_VARARGS", "ACC_TRANSIENT"},
    {"ACC_NATIVE"},
    {"ACC_INTERFACE"},
    {"ACC_ABSTRACT"},
    {"ACC_STRICT"},
    {"ACC_SYNTHETIC"},
    {"ACC_ANNOTATION"},
    {"ACC_ENUM"},
    {"ACC_MANDATED", "ACC_MODULE"}};
  uint64_t staticFieldCount = std::accumulate(
    flagNames.begin(), flagNames.end(),
    0,
    [](uint64_t totalCount, std::vector<std::string> v) { return totalCount + v.size(); });
  Keyword **staticFieldNames = (Keyword **) allocate(staticFieldCount * sizeof(Keyword *));
  Object **staticFields = (Object **) allocate(staticFieldCount * sizeof(Object *));
  for (size_t i = 0, k = 0; i < flagNames.size(); ++i)
    for (size_t j = 0; j < flagNames[i].size(); ++j) {
      staticFieldNames[k] = Keyword_create(String_createDynamicStr(flagNames[i][j].c_str()));
      staticFields[k] = super(Integer_create(1 << i));
      ++k;
    }
  return Class_create(name, name, staticFieldCount, staticFieldNames, staticFields, 0);
}

Class *clojureLangVar() {
  const char *s = "class clojure.lang.Var";
  String *name = String_createDynamicStr(s);
  retain(name);
  return Class_create(name, name, 0, NULL, NULL, 0);
}

Class *clojureLangVar__DOLLAR__Unbound() {
  const char *s = "class clojure.lang.Var$Unbound";
  String *name = String_createDynamicStr(s);
  retain(name);
  Keyword *varName = Keyword_create(String_createDynamicStr("varName"));
  Class *_class = Class_create(name, name, 0, NULL, NULL, 1, varName);
  UNIQUE_UnboundClass = _class;
  return _class;
}
