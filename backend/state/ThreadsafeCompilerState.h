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
  ThreadsafeRegistry<::Class> protocolRegistry;
  ThreadsafeRegistry<const Node> functionAstRegistry;

  ThreadsafeCompilerState();
  void initializeDefaultNamespaces();
  ~ThreadsafeCompilerState();

  void storeInternalClasses(RTValue from);
  void extendInternalClasses(RTValue from);
  void storeInternalProtocols(RTValue from);

  void validateProtocolImplementations(const std::vector<::Class *> &classes);

  Var *getOrCreateVar(const char *name);
  Var *getCurrentVar(const char *name);
  void registerVar(const char *name, Var *var);

private:
};
} // namespace rt

#endif
