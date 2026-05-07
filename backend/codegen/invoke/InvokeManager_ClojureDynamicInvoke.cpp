#include "../../bridge/Exceptions.h"
#include "../CodeGen.h"
#include "InvokeManager.h"
#include "types/ConstantFunction.h"

namespace rt {

TypedValue InvokeManager::generateDynamicInvoke(
    TypedValue fn, const std::vector<TypedValue> &args,
    CleanupChainGuard *guard, const clojure::rt::protobuf::bytecode::Node *node,
    const std::vector<TypedValue> &extraCleanup) {
  size_t argCount = args.size();
  llvm::Value *currentVal = valueEncoder.box(fn).value;

  // 1. Unified IC resolution (Handles Functions, Keywords, Maps, Vectors)
  llvm::Value *methodToCall = generateICLookup(currentVal, argCount, guard);

  // 2. Delegate to the raw call sequence
  // If methodToCall is NULL, generateRawMethodCall will handle the error/null check via structural GEP loads
  // but we should probably check for NULL here for better error reporting if we want.
  // Actually, generateRawMethodCall expects a valid methodPtr.
  
  return generateRawMethodCall(methodToCall, fn, args, guard, node, extraCleanup);
}

} // namespace rt
