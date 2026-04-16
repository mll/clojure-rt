#include "../CodeGen.h"
#include "bytecode.pb.h"
#include <string>

using namespace std;
using namespace llvm;
using namespace clojure::rt::protobuf::bytecode;

namespace rt {

TypedValue CodeGen::codegen(const Node &node, const RecurNode &subnode,
                            const ObjectTypeSet &typeRestrictions) {
  // 1. Find RecurTarget
  auto it = recurTargets.find(subnode.loopid());
  if (it == recurTargets.end()) {
    throwCodeGenerationException("Recur target not found: " + subnode.loopid(),
                                 node);
  }

  const RecurTarget &target = it->second;

  if (target.isVariadic && (target.numParams + 1) != subnode.exprs_size()) {
    cout << to_string(target.numParams) + " " + to_string(subnode.exprs_size())
         << endl;
    throwCodeGenerationException("Recur target has wrong number of arguments",
                                 node);
  }

  if (!target.isVariadic && target.numParams != subnode.exprs_size()) {
    throwCodeGenerationException("Recur target has wrong number of arguments",
                                 node);
  }

  // 2. Evaluate all recur expressions with protection
  CleanupChainGuard guard(*this);
  vector<TypedValue> newArgs;
  for (int i = 0; i < subnode.exprs_size(); ++i) {
    TypedValue t = codegen(subnode.exprs(i), ObjectTypeSet::all());
    newArgs.push_back(t);
    guard.push(t);
  }

  // 3. Apply unwindmemory guidance (releases surviving locals)
  for (int i = 0; i < node.unwindmemory_size(); i++) {
    memoryManagement.dynamicMemoryGuidance(node.unwindmemory(i));
  }

  // 4. Hand over ownership of newArgs to the tail call context.
  // We pop them from the backend's cleanup stack because the callee
  // (the next iteration) will start a fresh scope and take over.

  guard.popAll();

  // 5. Distribute arguments according to baseline calling convention
  // Baseline: RTValue (Frame*, RTValue r0, RTValue r1, RTValue r2, RTValue r3,
  // RTValue r4)

  vector<Value *> callArgs;
  callArgs.push_back(target.framePtr);

  // If the function is variadic, the last parameter of the function matches the
  // last expression of recur. We need to store it in frame->variadicSeq.
  if (target.isVariadic) {
    Value *variadicSeq = valueEncoder.box(newArgs.back()).value;
    Value *variadicSeqPtr = Builder.CreateStructGEP(
        types.frameTy, target.framePtr, 2, "variadicSeq_ptr");
    Builder.CreateStore(variadicSeq, variadicSeqPtr);
    newArgs.pop_back();
  } else {
    Value *variadicSeqPtr = Builder.CreateStructGEP(
        types.frameTy, target.framePtr, 2, "variadicSeq_ptr");
    Builder.CreateStore(valueEncoder.boxNil().value, variadicSeqPtr);
  }

  // First 5 arguments go to registers (r0-r4)
  for (int i = 0; i < 5; ++i) {
    if (i < (int)newArgs.size()) {
      callArgs.push_back(valueEncoder.box(newArgs[i]).value);
    } else {
      callArgs.push_back(valueEncoder.boxNil().value);
    }
  }

  // Arguments beyond index 4 go to the frame
  for (int i = 5; i < (int)newArgs.size(); ++i) {
    Value *val = valueEncoder.box(newArgs[i]).value;
    // Accessing element i-5 in the flexible array (field 5 of Clojure_Frame)
    Value *argPtr = Builder.CreateInBoundsGEP(
        types.frameTy, target.framePtr,
        {Builder.getInt32(0), Builder.getInt32(5), Builder.getInt32(i - 5)},
        "arg_ptr");
    Builder.CreateStore(val, argPtr);
  }
  // 6. Emit EBR checkpoint (unconditionally)
  // TODO - maybe some metrics should tell us we should emit or not?

  llvm::FunctionType *flushTy =
      llvm::FunctionType::get(types.voidTy, {}, false);
  invokeManager.invokeRaw("Ebr_flush_critical", flushTy,
                          std::vector<llvm::Value *>{});

  // 7. Emit musttail call
  CallInst *call =
      Builder.CreateCall(types.baselineFunctionTy, target.function, callArgs);
  call->setTailCallKind(CallInst::TCK_MustTail);

  Builder.CreateRet(call);

  // To prevent trailing instructions from polluting the basic block (since
  // musttail requires it to end with ret), we create a dead block for the
  // remainder of the AST codegen to write into.
  llvm::BasicBlock *deadBB = llvm::BasicBlock::Create(
      TheContext, "dead_recur", Builder.GetInsertBlock()->getParent());
  Builder.SetInsertPoint(deadBB);

  return TypedValue(ObjectTypeSet::dynamicType(), valueEncoder.boxNil().value);
}

ObjectTypeSet CodeGen::getType(const Node &node, const RecurNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
  // recur never returns to the caller
  return ObjectTypeSet::dynamicType();
}

} // namespace rt
