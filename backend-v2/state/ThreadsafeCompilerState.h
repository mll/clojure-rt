#ifndef THREADSAFE_COMPILER_STATE_H
#define THREADSAFE_COMPILER_STATE_H

#include "ThreadsafeRegistry.h"
#include "ThreadsafeInlineCacheManager.h"

extern "C" {
#include "runtime/ObjectProto.h"
#include "runtime/Object.h"
#include "runtime/Class.h"
#include "runtime/PersistentArrayMap.h"
/* #include "runtime/Var.h" */
}

using namespace clojure::rt::protobuf::bytecode;

namespace rt {
  class ThreadsafeCompilerState {
  public:
    
    ThreadsafeInlineCacheManager objectFieldIndexAccessInlineCache;
    ThreadsafeInlineCacheManager methodCallInlineCache;

    ThreadsafeRegistry<Class> classRegistry;
    ThreadsafeRegistry<Node> functionAstRegistry;
    ThreadsafeRegistry<PersistentArrayMap> internalClassRegistry;
    ThreadsafeRegistry<PersistentArrayMap> internalProtocolRegistry;    
    /* ThreadsafeRegistry<Var> varRegistry; */


    ThreadsafeCompilerState()
        : classRegistry(true), functionAstRegistry(false),
          internalClassRegistry(true),
          internalProtocolRegistry(true) /*, varRegistry(true)*/ {}

    void storeInternalClasses(RTValue from);
    void storeInternalProtocols(RTValue from);        
  };  
}

#endif
