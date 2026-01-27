#ifndef THREADSAFE_COMPILER_STATE_H
#define THREADSAFE_COMPILER_STATE_H

#include "ThreadsafeRegistry.h"
#include "ThreadsafeInlineCacheManager.h"

extern "C" {
#include "runtime/ObjectProto.h"
#include "runtime/Object.h"
#include "runtime/Class.h"
/* #include "runtime/Var.h" */
}

using namespace clojure::rt::protobuf::bytecode;

namespace rt {
  class ThreadsafeCompilerState {
  public:
    
    ThreadsafeInlineCacheManager objectFieldIndexAccessInlineCache;
    ThreadsafeInlineCacheManager methodCallInlineCache;

    ThreadsafeRegistry<Class> classRegistry;
    /* ThreadsafeRegistry<Var> varRegistry; */

    ThreadsafeRegistry<Node> functionAstRegistry;
    
    ThreadsafeCompilerState(): classRegistry(true), functionAstRegistry(false)/*, varRegistry(true)*/ {}
  };  
}

#endif
