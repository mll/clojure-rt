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
  void **packPointerArgs(uint64_t count, ...);
}

Class *createJavaLangObject(uint64_t funId) {
  const char *s = "class java.lang.Object";
  String *name = String_createDynamicStr(s);
  retain(name);
  std::vector<std::string> methodNames {"toString"};
  Keyword **keywordMethodNames = (Keyword **) allocate(methodNames.size() * sizeof(Keyword *));
  for (size_t i = 0; i < methodNames.size(); ++i) {
    keywordMethodNames[i] = Keyword_create(String_createDynamicStr(methodNames[i].c_str()));
  }
  ClojureFunction **methods = (ClojureFunction **) allocate(methodNames.size() * sizeof(ClojureFunction *));
  ClojureFunction *method = Function_create(1, funId, 1, false);
  std::string loopId = "java.lang.Object/toString";
  Function_fillMethod(method, 0, 0, 1, FALSE, strdup(loopId.c_str()), 0);
  methods[0] = method; // TODO: toString
  return Class_create(
    name, name, 0,
    NULL, // no superclass!
    0, NULL, NULL,
    0, NULL, NULL,
    0, NULL,
    methodNames.size(), keywordMethodNames, methods,
    0, NULL, NULL
  );
}

Class *createJavaLangClass(Class *javaLangObject) {
  const char *s = "class java.lang.Class";
  String *name = String_createDynamicStr(s);
  retain(name);
  return Class_create(
    name, name, 0, javaLangObject,
    0, NULL, NULL,
    0, NULL, NULL,
    0, NULL,
    0, NULL, NULL,
    0, NULL, NULL
  );
}

Class *createJavaLangLong(Class *javaLangNumber) {
  const char *s = "class java.lang.Long";
  String *name = String_createDynamicStr(s);
  retain(name);
  std::vector<std::string> fieldNames {"BYTES", "MAX_VALUE", "MIN_VALUE", "SIZE" /*, "TYPE"*/};
  Keyword **staticFieldNames = (Keyword **) allocate(fieldNames.size() * sizeof(Keyword *));
  for (size_t i = 0; i < fieldNames.size(); ++i) staticFieldNames[i] = Keyword_create(String_createDynamicStr(fieldNames[i].c_str()));
  Object **staticFields = (Object **) allocate(fieldNames.size() * sizeof(Object *));
  staticFields[0] = (Object *)Integer_create(8);
  staticFields[1] = (Object *)Integer_create(INT64_MAX);
  staticFields[2] = (Object *)Integer_create(INT64_MIN);
  staticFields[3] = (Object *)Integer_create(64);
  // staticFields[4] = ???
  return Class_create(
    name, name, 0,
    javaLangNumber,
    fieldNames.size(), staticFieldNames, staticFields,
    0, NULL, NULL,
    0, NULL,
    0, NULL, NULL,
    0, NULL, NULL
  );
}

Class *createClojureAsmOpcodes() {
  const char *s = "class clojure.asm.Opcodes";
  String *name = String_createDynamicStr(s);
  retain(name);
  std::vector<std::vector<std::string>> flagNames {
    {"ACC_PUBLIC"}, // 0x0001
    {"ACC_PRIVATE"}, // 0x0002
    {"ACC_PROTECTED"}, // 0x0004
    {"ACC_STATIC"}, // 0x0008
    {"ACC_FINAL"}, // 0x0010
    {"ACC_SUPER", "ACC_SYNCHRONIZED", "ACC_OPEN", "ACC_TRANSITIVE"}, // 0x0020
    {"ACC_VOLATILE", "ACC_BRIDGE", "ACC_STATIC_PHASE"}, // 0x0040
    {"ACC_VARARGS", "ACC_TRANSIENT"}, // 0x0080
    {"ACC_NATIVE"}, // 0x0100
    {"ACC_INTERFACE"}, // 0x0200
    {"ACC_ABSTRACT"}, // 0x0400
    {"ACC_STRICT"}, // 0x0800
    {"ACC_SYNTHETIC"}, // 0x1000
    {"ACC_ANNOTATION"}, // 0x2000
    {"ACC_ENUM"}, // 0x4000
    {"ACC_MANDATED", "ACC_MODULE"}}; // 0x8000
  uint64_t flagCount = std::accumulate(
    flagNames.begin(), flagNames.end(),
    0,
    [](uint64_t totalCount, std::vector<std::string> v) { return totalCount + v.size(); });
  
  std::vector<std::pair<std::string, int64_t>> javaClassFileVersions {
    {"V1_1", 3 << 16 | 45},
    {"V1_2", 0 << 16 | 46},
    {"V1_3", 0 << 16 | 47},
    {"V1_4", 0 << 16 | 48},
    {"V1_5", 0 << 16 | 49},
    {"V1_6", 0 << 16 | 50},
    {"V1_7", 0 << 16 | 51},
    {"V1_8", 0 << 16 | 52},
    {"V9", 0 << 16 | 53},
    {"V10", 0 << 16 | 54},
    {"V11", 0 << 16 | 55}
  };
  uint64_t versionsCount = javaClassFileVersions.size();
  uint64_t staticFieldCount = flagCount + versionsCount;
  
  Keyword **staticFieldNames = (Keyword **) allocate(staticFieldCount * sizeof(Keyword *));
  Object **staticFields = (Object **) allocate(staticFieldCount * sizeof(Object *));
  size_t k = 0;
  for (size_t i = 0; i < flagNames.size(); ++i)
    for (size_t j = 0; j < flagNames[i].size(); ++j) {
      staticFieldNames[k] = Keyword_create(String_createDynamicStr(flagNames[i][j].c_str()));
      staticFields[k] = (Object *)Integer_create(1 << i);
      ++k;
    }
  for (size_t i = 0; i < versionsCount; ++i) {
    staticFieldNames[k] = Keyword_create(String_createDynamicStr(javaClassFileVersions[i].first.c_str()));
    staticFields[k] = (Object *)Integer_create(javaClassFileVersions[i].second);
    ++k;
  }
  return Class_create(
    name, name, 0x0200,
    NULL, // No superclass - this is an interface!
    staticFieldCount, staticFieldNames, staticFields,
    0, NULL, NULL,
    0, NULL,
    0, NULL, NULL,
    0, NULL, NULL
  );
}

Class *createClojureLangVar(Class *clojureLangARef) {
  const char *s = "class clojure.lang.Var";
  String *name = String_createDynamicStr(s);
  retain(name);
  return Class_create(
    name, name, 0,
    clojureLangARef,
    0, NULL, NULL,
    0, NULL, NULL,
    0, NULL,
    0, NULL, NULL,
    0, NULL, NULL
  );
}

Class *createClojureLangVar__DOLLAR__Unbound(Class *clojureLangAFn) {
  const char *s = "class clojure.lang.Var$Unbound";
  String *name = String_createDynamicStr(s);
  retain(name);
  Keyword *varName = Keyword_create(String_createDynamicStr("varName"));
  Class *_class = Class_create(
    name, name, 0,
    clojureLangAFn,
    0, NULL, NULL,
    0, NULL, NULL,
    1, (Keyword **) packPointerArgs(1, varName),
    0, NULL, NULL,
    0, NULL, NULL
  );
  UNIQUE_UnboundClass = _class;
  return _class;
}

Class *createClojureLangCompiler(Class *javaLangObject) {
  const char *s = "class clojure.lang.Compiler";
  String *name = String_createDynamicStr(s);
  retain(name);
  return Class_create(
    name, name, 0,
    javaLangObject,
    0, NULL, NULL,
    0, NULL, NULL,
    0, NULL,
    0, NULL, NULL,
    0, NULL, NULL
  );
}
