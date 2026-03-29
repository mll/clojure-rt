#ifndef THREADSAFE_COMPILER_STATE_H
#define THREADSAFE_COMPILER_STATE_H

#include "../RuntimeHeaders.h"
#include "ThreadsafeInlineCacheManager.h"
#include "ThreadsafeRegistry.h"
#include "bytecode.pb.h"

extern "C" {
#include "runtime/Class.h"
#include "runtime/Object.h"
#include "runtime/ObjectProto.h"
#include "runtime/PersistentArrayMap.h"
#include "runtime/Var.h"
}

using namespace clojure::rt::protobuf::bytecode;

namespace rt {
class ThreadsafeCompilerState {
public:
  ThreadsafeRegistry<::Class> classRegistry;
  ThreadsafeRegistry<const Node> functionAstRegistry;
  ThreadsafeRegistry<::Var> varRegistry;

  ThreadsafeCompilerState()
      : classRegistry(true, 1000), functionAstRegistry(false),
        varRegistry(true) {}

  void storeInternalClasses(RTValue from);
  void storeInternalProtocols(RTValue from);
};
} // namespace rt

#endif
