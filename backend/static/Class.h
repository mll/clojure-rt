#ifndef RT_STATIC_CLASS
#define RT_STATIC_CLASS
#include "../codegen.h"

extern "C" {
  #include "runtime/Class.h"
}

std::unordered_map<std::string, Class *> getStaticClasses();

#endif
