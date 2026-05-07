#include "../../tools/RTValueWrapper.h"
#include "../CodeGen.h"
#include "InvokeManager.h"
#include "llvm/IR/MDBuilder.h"
#include <atomic>
#include <sstream>

using namespace llvm;
using namespace std;

namespace rt {

TypedValue InvokeManager::generateVarInvoke(
    TypedValue varObj, const std::vector<TypedValue> &args,
    CleanupChainGuard *guard,
    const clojure::rt::protobuf::bytecode::Node *node) {

  size_t argCount = args.size();

  // 1. Get Var pointer (unboxed)
  Value *varPtr = valueEncoder.unboxPointer(varObj).value;

  // 2. Load current value FROM VAR (borrowed call - Var_peek)
  FunctionType *peekSig = FunctionType::get(
      types.RT_valueTy, {types.ExecutionContextPtrTy, types.ptrTy}, false);
  Value *currentVal =
      invokeRaw("Var_peek", peekSig, {codeGen.getExecutionContext(), varPtr},
                guard, false, {}, true);


  // 3. Unified IC resolution (Handles Functions, Keywords, Maps, Vectors)
  Value *methodToCall = generateICLookup(currentVal, argCount, guard);

  // 4. Perform the actual call
  return generateRawMethodCall(
      methodToCall, TypedValue(ObjectTypeSet::dynamicType(), currentVal), args,
      guard, node);
}

} // namespace rt
