#ifndef RT_STATIC_CLASS
#define RT_STATIC_CLASS
#include "../codegen.h"

extern "C" {
  #include "runtime/Class.h"
}

Class *createJavaLangObject(uint64_t funId);
Class *createJavaLangClass(Class *javaLangObject);
Class *createJavaLangLong(Class *javaLangNumber);
Class *createClojureAsmOpcodes();
Class *createClojureLangVar(Class *clojureLangARef);
Class *createClojureLangVar__DOLLAR__Unbound(Class *clojureLangAFn);
Class *createClojureLangCompiler(Class *javaLangObject);

#endif
