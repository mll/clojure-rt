#include "../CodeGen.h"
#include "../MemoryManagement.h"
#include "../invoke/InvokeManager.h"
#include "bridge/ClassLookup.h"
#include "bridge/Exceptions.h"
#include "runtime/ObjectProto.h"

using namespace std;
using namespace llvm;

namespace rt {

TypedValue CodeGen::codegen(const Node &node, const TryNode &subnode,
                            const ObjectTypeSet &typeRestrictions) {
  throwCodeGenerationException("Throw node not implemented yet", node);
  // llvm::Function *F = Builder.GetInsertBlock()->getParent();

  // llvm::BasicBlock *finallyBB = BasicBlock::Create(TheContext, "finally", F);
  // llvm::BasicBlock *endTryBB = BasicBlock::Create(TheContext, "end_try", F);

  // llvm::Value *resultAlloca = nullptr;

  // // Create all necessary allocas in the entry block
  // llvm::BasicBlock &entryBB = F->getEntryBlock();
  // llvm::IRBuilder<> entryBuilder(&entryBB, entryBB.begin());
  // entryBuilder.SetCurrentDebugLocation(Builder.getCurrentDebugLocation());

  // resultAlloca = entryBuilder.CreateAlloca(types.RT_valueTy, nullptr,
  // "try_result");
  // // Default initialize result to nil
  // TypedValue nilResult = dynamicConstructor.createNil();
  // Builder.CreateStore(valueEncoder.box(nilResult).value, resultAlloca);

  // // Slot to store the active exception during unwind handling
  // llvm::Value *unwindExnSlot = entryBuilder.CreateAlloca(
  //     llvm::StructType::get(TheContext, {types.ptrTy, types.i32Ty}), nullptr,
  //     "unwind_exn_slot");

  // 1. Setup Finally Unwind Destination
  //    This landing pad catches exceptions thrown by the try block (if no
  //    catches match) or by the catch blocks (nested throw). It ensures
  //    `finally` is executed during stack unwind.
  //
  //    C++ ABI NOTE: When we use LLVM exceptions that map to the Itanium C++
  //    ABI, the exception stack needs to be properly maintained. Specifically,
  //    whenever we intercept an exception (like catching it), we must call
  //    `__cxa_begin_catch` and `__cxa_end_catch`. If a nested `throw` happens
  //    during evaluation of `try`, it unwinds the stack. We push
  //    `finallyUnwindBB` to the memory management stack so that `invoke`
  //    instructions generated within the `try` block know where to unwind to.
  // llvm::BasicBlock *finallyUnwindBB = nullptr;
  // llvm::BasicBlock *finallyCleanupBodyBB = nullptr;
  // if (subnode.has_finally()) {
  //     finallyUnwindBB = BasicBlock::Create(TheContext, "finally_unwind", F);
  //     finallyCleanupBodyBB = BasicBlock::Create(TheContext,
  //     "finally_cleanup_body", F);
  //     memoryManagement.pushExceptionDestination(finallyUnwindBB);
  // }

  // // 2. Setup Catch Dispatcher Destination
  // llvm::BasicBlock *catchDispatcherBB = BasicBlock::Create(TheContext,
  // "catch_dispatcher", F);
  // memoryManagement.pushExceptionDestination(catchDispatcherBB);

  // // 3. Compile Try Body
  // TypedValue tryResult = codegen(subnode.body(), typeRestrictions);
  // Builder.CreateStore(valueEncoder.box(tryResult).value, resultAlloca);

  // if (subnode.allcatchesowned_size() > 0) {
  //     for (int i = 0; i < subnode.allcatchesowned_size(); i++) {
  //         memoryManagement.dynamicMemoryGuidance(subnode.allcatchesowned(i));
  //     }
  // }
  // Builder.CreateBr(finallyBB);

  // // 4. Pop Catch Dispatcher
  // memoryManagement.popExceptionDestination();

  // // Prepare Catch Blocks Unwind Destination
  // llvm::BasicBlock *catchUnwindBB = nullptr;
  // if (subnode.catches_size() > 0) {
  //     catchUnwindBB = BasicBlock::Create(TheContext, "catch_unwind", F);
  // }

  // // 5. Catch Dispatcher
  // Builder.SetInsertPoint(catchDispatcherBB);
  // memoryManagement.ensureExceptionInfrastructure(F);
  // llvm::Value *exSlotVal = Builder.CreateLoad(
  //     llvm::StructType::get(TheContext, {types.ptrTy, types.i32Ty}),
  //     memoryManagement.getExceptionSlot());

  // entryBuilder.SetInsertPoint(&F->getEntryBlock(),
  // F->getEntryBlock().getFirstInsertionPt()); llvm::Value *capturedExAlloca =
  // entryBuilder.CreateAlloca(types.RT_valueTy, nullptr, "captured_ex_slot");
  // Builder.CreateStore(llvm::ConstantInt::get(types.RT_valueTy, 0),
  // capturedExAlloca);

  // llvm::Value *exPtr = Builder.CreateExtractValue(exSlotVal, 0);

  // // Officially catch the exception via C++ ABI (increments handler count)
  // llvm::Value *beginCatchRes = invokeManager.invokeRaw("RT_cxa_begin_catch",
  // llvm::FunctionType::get(types.ptrTy, {types.ptrTy}, false), {exPtr});

  // // Convert C++ exception to Clojure exception
  // llvm::Value *capExVal = invokeManager.invokeRaw("RT_captureException",
  // llvm::FunctionType::get(types.RT_valueTy, {types.ptrTy}, false),
  // {beginCatchRes}); Builder.CreateStore(capExVal, capturedExAlloca);
  // TypedValue capturedEx(ObjectTypeSet::all().boxed(), capExVal);

  // llvm::BasicBlock *currentCatchCheckBB = Builder.GetInsertBlock();
  // std::vector<llvm::BasicBlock*> catchBlocks;
  // std::vector<llvm::BasicBlock*> catchCheckBlocks;

  // llvm::BasicBlock *rethrowBB = BasicBlock::Create(TheContext, "rethrow", F);

  // for (int i = 0; i < subnode.catches_size(); ++i) {
  //     catchBlocks.push_back(BasicBlock::Create(TheContext, "catch_body_" +
  //     std::to_string(i), F)); if (i < subnode.catches_size() - 1) {
  //         catchCheckBlocks.push_back(BasicBlock::Create(TheContext,
  //         "catch_check_" + std::to_string(i + 1), F));
  //     } else {
  //         catchCheckBlocks.push_back(rethrowBB);
  //     }
  // }

  // if (subnode.catches_size() == 0) {
  //     Builder.CreateBr(rethrowBB);
  // }

  // for (int i = 0; i < subnode.catches_size(); ++i) {
  //     const auto& catchClauseAST = subnode.catches(i);
  //     const auto& catchClause = catchClauseAST.subnode().catch_();
  //     Builder.SetInsertPoint(currentCatchCheckBB);

  //     string className = catchClause.class_().subnode().const_().val();
  //     if (className.find("class ") == 0) {
  //       className = className.substr(6);
  //     } else if (className.find("interface ") == 0) {
  //       className = className.substr(10);
  //     }

  //     llvm::Value *classNameGlobal = Builder.CreateGlobalString(className,
  //     "is_instance_class"); llvm::Value *jitEngineVal =
  //     Builder.CreateIntToPtr(llvm::ConstantInt::get(types.i64Ty,
  //     (uintptr_t)jitEnginePtr), types.ptrTy);

  //     llvm::FunctionType *isInstSig = llvm::FunctionType::get(types.i1Ty,
  //     {types.ptrTy, types.ptrTy, types.RT_valueTy}, false);
  //     memoryManagement.dynamicRetain(capturedEx);

  //     llvm::Value *matchBool = invokeManager.invokeRaw(
  //         "Class_isInstanceClassName", isInstSig,
  //         {classNameGlobal, jitEngineVal, capturedEx.value});

  //     Builder.CreateCondBr(matchBool, catchBlocks[i], catchCheckBlocks[i]);

  //     // Compile Catch Body
  //     Builder.SetInsertPoint(catchBlocks[i]);

  //     // Push catchUnwindBB so that if the catch body throws, we execute
  //     __cxa_end_catch if (catchUnwindBB) {
  //         memoryManagement.pushExceptionDestination(catchUnwindBB);
  //     }

  //     const auto& localNode = catchClause.local().subnode().binding();
  //     variableBindingStack.push();
  //     variableBindingStack.set(localNode.name(), capturedEx);
  //     variableTypesBindingsStack.push();
  //     variableTypesBindingsStack.set(localNode.name(), capturedEx.type);

  //     TypedValue catchResult = codegen(catchClause.body(), typeRestrictions);

  //     variableBindingStack.pop();
  //     variableTypesBindingsStack.pop();
  //     Builder.CreateStore(valueEncoder.box(catchResult).value, resultAlloca);

  //     if (catchUnwindBB) {
  //         memoryManagement.popExceptionDestination();
  //     }

  //     // We successfully finished the catch block without throwing.
  //     // Therefore, we must call __cxa_end_catch() to decrement the C++ ABI
  //     handler count. invokeManager.invokeRuntime("RT_cxa_end_catch", nullptr,
  //     {}, {});

  //     Builder.CreateBr(finallyBB);

  //     currentCatchCheckBB = catchCheckBlocks[i];
  // }

  // // 6. Rethrow Block (no catches matched)
  // Builder.SetInsertPoint(rethrowBB);
  // llvm::Value *loadedEx = Builder.CreateLoad(types.RT_valueTy,
  // capturedExAlloca);
  // memoryManagement.dynamicRelease(TypedValue(ObjectTypeSet::all().boxed(),
  // loadedEx));
  // // RT_cxa_rethrow will throw the active exception.
  // // Since finallyUnwindBB is STILL on the stack, it will unwind to
  // finallyUnwindBB and run `finally`.
  // invokeManager.invokeRaw("RT_cxa_rethrow",
  // llvm::FunctionType::get(types.voidTy, {}, false), {});
  // Builder.CreateUnreachable();

  // // 7. Pop Finally Unwind Destination
  // // All code that should trigger finally during unwind has been generated.
  // if (finallyUnwindBB) {
  //     memoryManagement.popExceptionDestination();
  // }

  // // 8. Catch Unwind Landing Pad (if a catch body throws a new exception)
  // //    C++ ABI NOTE: This is extremely important to prevent memory leaks and
  // `std::terminate`.
  // //    When a C++ exception is caught (via `__cxa_begin_catch` in the Catch
  // Dispatcher),
  // //    the C++ runtime marks the exception as "handled". To free the
  // exception memory
  // //    and decrement the handler count, `__cxa_end_catch` must be called.
  // //    If the catch body itself throws a *new* exception, we unwind the
  // stack.
  // //    Without a dedicated cleanup landing pad here, the stack unwinds
  // directly out of the
  // //    catch block without calling `__cxa_end_catch`, causing the original
  // exception to leak.
  // //    This landing pad explicitly catches unwinds originating from the
  // catch body,
  // //    calls `__cxa_end_catch` to finalize the first exception, and then
  // branches to the
  // //    next active landing pad to continue unwinding the new exception.
  // if (catchUnwindBB) {
  //     Builder.SetInsertPoint(catchUnwindBB);
  //     // The exception is already captured into exceptionSlot by
  //     getLandingPad().
  //     // Clean up the original exception that we were handling when the new
  //     throw occurred. invokeManager.invokeRuntime("RT_cxa_end_catch",
  //     nullptr, {}, {});

  //     if (finallyCleanupBodyBB) {
  //         Builder.CreateBr(finallyCleanupBodyBB);
  //     } else {
  //         llvm::Value *exSlotVal = Builder.CreateLoad(
  //             llvm::StructType::get(TheContext, {types.ptrTy, types.i32Ty}),
  //             memoryManagement.getExceptionSlot());
  //         Builder.CreateResume(exSlotVal);
  //     }
  // }

  // // 9. Finally Unwind Destination (if try throws or catch rethrows)
  // if (finallyUnwindBB) {
  //     Builder.SetInsertPoint(finallyUnwindBB);
  //     Builder.CreateBr(finallyCleanupBodyBB);

  //     // The body where finally AST is evaluated during unwind.
  //     Builder.SetInsertPoint(finallyCleanupBodyBB);
  //     codegen(subnode.finally(), ObjectTypeSet::all());
  //     llvm::Value *exSlotVal = Builder.CreateLoad(
  //         llvm::StructType::get(TheContext, {types.ptrTy, types.i32Ty}),
  //         memoryManagement.getExceptionSlot());
  //     Builder.CreateResume(exSlotVal);
  // }

  // // 10. Normal Finally Block Execution
  // Builder.SetInsertPoint(finallyBB);
  // if (subnode.has_finally()) {
  //     codegen(subnode.finally(), ObjectTypeSet::all());
  // }
  // Builder.CreateBr(endTryBB);

  // // 11. End Try
  // Builder.SetInsertPoint(endTryBB);
  // llvm::Value *finalResultPacked = Builder.CreateLoad(types.RT_valueTy,
  // resultAlloca);

  // return TypedValue(getType(node, subnode, typeRestrictions),
  // finalResultPacked);
}

ObjectTypeSet CodeGen::getType(const Node &node, const TryNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
  throwCodeGenerationException("Throw node not implemented yet", node);
  // ObjectTypeSet result = getType(subnode.body(), typeRestrictions);
  // for (int i = 0; i < subnode.catches_size(); ++i) {
  //     const auto& catchClause = subnode.catches(i).subnode().catch_();
  //     const auto& localNode = catchClause.local().subnode().binding();

  //     variableTypesBindingsStack.push();
  //     variableTypesBindingsStack.set(localNode.name(),
  //     ObjectTypeSet::all().boxed());

  //     result = result.expansion(getType(catchClause.body(),
  //     typeRestrictions));

  //     variableTypesBindingsStack.pop();
  // }
  // return result;
}

} // namespace rt
