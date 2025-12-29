#ifndef THREADSAFE_COMPILER_STATE_H
#define THREADSAFE_COMPILER_STATE_H

#include "ThreadsafeRegistry.h"
#include "ThreadsafeInlineCacheManager.h"

extern "C" {
#include "runtime/ObjectProto.h"
#include "runtime/Object.h"
#include "runtime/Class.h"
#include "runtime/Interface.h"
/* #include "runtime/Var.h" */
}

using namespace clojure::rt::protobuf::bytecode;

namespace rt {
  class ThreadsafeCompilerState {
  public:
    static ThreadsafeCompilerState &get() {
      // Threadsafe from C++11 up      
      static ThreadsafeCompilerState instance;
      return instance;
    }

    ThreadsafeInlineCacheManager objectFieldIndexAccessCache;
    ThreadsafeInlineCacheManager methodCallCache;

    ThreadsafeRegistry<Class> classRegistry;
    ThreadsafeRegistry<Interface> interfaceRegistry;
    /* ThreadsafeRegistry<Var> varRegistry; */

    ThreadsafeRegistry<Node> functionAstRegistry;
    
    // Removing original implementation to keep the singleton safe.
    ThreadsafeCompilerState(const ThreadsafeCompilerState&) = delete;
    void operator=(const ThreadsafeCompilerState&) = delete;
    ThreadsafeCompilerState(ThreadsafeCompilerState&&) = delete;
    void operator=(ThreadsafeCompilerState &&) = delete;
  private:
    ThreadsafeCompilerState(): classRegistry(true), interfaceRegistry(true), functionAstRegistry(false)/*, varRegistry(true)*/ {}
  };  
}

#endif
