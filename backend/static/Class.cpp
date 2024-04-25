extern "C" {
  #include "runtime/Class.h"
  #include "runtime/String.h"
  void retain(void *obj);
}

Class *javaLangClass() {
    const char* s = "class java.lang.Class";
    String* name = String_createDynamicStr(s);
    retain(name);
    return Class_create(name, name, 0);
}
