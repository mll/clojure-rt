#ifndef RT_STATIC_CLASS
#define RT_STATIC_CLASS
#include "../codegen.h"

extern "C" {
  #include "runtime/Class.h"
}

Class *javaLangClass();
Class *javaLangLong();
Class *clojureAsmOpcodes();
Class *clojureLangVar();
Class *clojureLangVar__DOLLAR__Unbound();

#endif
