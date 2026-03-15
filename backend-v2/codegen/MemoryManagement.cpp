#include "MemoryManagement.h"
#include "CodeGen.h"
#include "../RuntimeHeaders.h"
#include "../bridge/Exceptions.h"
#include "../cljassert.h"
#include "../runtime/word.h"
#include "CleanupChainGuard.h"
#include "DynamicConstructor.h"
#include "LLVMTypes.h"
#include "TypedValue.h"
#include "ValueEncoder.h"
#include "VariableBindings.h"
#include "llvm/ADT/StringSwitch.h"
#include <cstdint>
#include <llvm/IR/Constants.h>
#include <string>

using namespace llvm;

namespace rt {

MemoryManagement::MemoryManagement(llvm::LLVMContext &c, IRBuilder<> &b,
                                   llvm::Module &m,
                                   ValueEncoder &v, LLVMTypes &t,
                                   VariableBindings<TypedValue> &vb,
                                   InvokeManager &i)
    : context(c), builder(b), theModule(m), valueEncoder(v), types(t),
      variableBindingStack(vb), invoke(i) {}

void MemoryManagement::initFunction(llvm::Function *F) {
  exceptionSlot = nullptr;
  terminalResumeBB = nullptr;
  clear();
}

void MemoryManagement::ensureExceptionInfrastructure(llvm::Function *F) {
  if (exceptionSlot) return;

  auto currentIP = builder.saveIP();

  // Create alloca in entry block if possible, or just here
  exceptionSlot = builder.CreateAlloca(llvm::StructType::get(context, {types.ptrTy, types.i32Ty}), nullptr, "exception_slot");
  terminalResumeBB = llvm::BasicBlock::Create(context, "terminal_resume", F);
  
  builder.SetInsertPoint(terminalResumeBB);
  llvm::Value *exVal = builder.CreateLoad(llvm::StructType::get(context, {types.ptrTy, types.i32Ty}), exceptionSlot, "exception_to_resume");
  builder.CreateResume(exVal);

  builder.restoreIP(currentIP);
}

void MemoryManagement::pushResource(TypedValue val) {
  activeResources.push_back(val);
  totalPushedResources++;
}

void MemoryManagement::popResource() {
  TypedValue val = activeResources.back();
  bool processed = activeResources.size() == resourcesWithCleanup;

  if (processed) {
    if (CleanupChainGuard::needsProtection(val.type)) {
      cleanupStack.pop_back();
    }
    resourcesWithCleanup--;
  }

  activeResources.pop_back();
}

llvm::BasicBlock *MemoryManagement::getLandingPad(size_t skipCount) {
  // 1. Calculate how many resources effectively need protection (survivors)
  size_t survivorCount = (skipCount < activeResources.size()) 
                          ? (activeResources.size() - skipCount) 
                          : 0;
  
  size_t neededSurvivors = 0;
  for (size_t i = 0; i < survivorCount; ++i) {
    if (CleanupChainGuard::needsProtection(activeResources[i].type)) {
      neededSurvivors++;
    }
  }

  // 2. If no survivors need protection, we don't need a landing pad or any blocks.
  if (neededSurvivors == 0)
    return nullptr;

  // 3. We are going to emit a landing pad, so ensure we have the infrastructure.
  llvm::Function *F = builder.GetInsertBlock()->getParent();
  ensureExceptionInfrastructure(F);

  // 4. Flush resources up to the end of the survivor zone (or further if needed)
  // We MUST ensure all survivors have blocks. We CAN skip building blocks for
  // the 'skipped' resources for now - they will be built only if some future
  // call needs them.
  while (resourcesWithCleanup < survivorCount) {
    TypedValue val = activeResources[resourcesWithCleanup];
    if (CleanupChainGuard::needsProtection(val.type)) {
      llvm::BasicBlock *prevCleanup =
          cleanupStack.empty() ? terminalResumeBB : cleanupStack.back();
      
      // Note: totalPushedResources is a global counter, activeResources.size() is local.
      // This naming might be slightly off in the original, but we follow the pattern.
      llvm::BasicBlock *newCleanup = llvm::BasicBlock::Create(
          context,
          "cleanup_link_" + std::to_string(totalPushedResources -
                                          (activeResources.size() -
                                           resourcesWithCleanup - 1)),
          F);

      auto currentIP = builder.saveIP();
      builder.SetInsertPoint(newCleanup);

      llvm::FunctionType *releaseFnTy =
          llvm::FunctionType::get(types.RT_valueTy, {types.RT_valueTy}, false);
      llvm::FunctionCallee releaseFn =
          theModule.getOrInsertFunction("release", releaseFnTy);
      builder.CreateCall(releaseFn, {valueEncoder.box(val).value});
      builder.CreateBr(prevCleanup);

      builder.restoreIP(currentIP);
      cleanupStack.push_back(newCleanup);
    }
    resourcesWithCleanup++;
  }

  // 5. The landing pad branches to the top of the 'survivor' cleanup stack.
  size_t survivorBlocks = 0;
  for (size_t i = 0; i < survivorCount; ++i) {
    if (CleanupChainGuard::needsProtection(activeResources[i].type)) {
      survivorBlocks++;
    }
  }

  assert(survivorBlocks > 0 && survivorBlocks <= cleanupStack.size());

  llvm::BasicBlock *lpadEntry = llvm::BasicBlock::Create(
      context, "lpad_entry_" + std::to_string(totalPushedResources), F);

  auto currentIP = builder.saveIP();
  builder.SetInsertPoint(lpadEntry);

  llvm::LandingPadInst *lpad = builder.CreateLandingPad(
      llvm::StructType::get(context, {types.ptrTy, types.i32Ty}), 1);
  lpad->setCleanup(true);

  builder.CreateStore(lpad, exceptionSlot);
  builder.CreateBr(cleanupStack[survivorBlocks - 1]);

  builder.restoreIP(currentIP);
  return lpadEntry;
}

void MemoryManagement::clear() {
  cleanupStack.clear();
  activeResources.clear();
  totalPushedResources = 0;
  resourcesWithCleanup = 0;
}

void MemoryManagement::dynamicMemoryGuidance(
    const MemoryManagementGuidance &guidance) {
  auto name = guidance.variablename();
  auto change = guidance.requiredrefcountchange();

  for (word_t depth = variableBindingStack.stackDepth() - 1; depth >= 0;
       depth--) {
    auto val = variableBindingStack.find(name, depth);
    if (val) {
      while (change != 0) {
        if (change > 0) {
          dynamicRetain(*val);
          change--;
        } else {
          dynamicRelease(*val);
          change++;
        }
      }
      break;
    }
  }
}

void MemoryManagement::dynamicRetain(TypedValue &target) {
  Metadata *metaPtr = dyn_cast<Metadata>(MDString::get(context, "retain"));
  MDNode *meta = MDNode::get(context, metaPtr);

  TypedValue retain = invoke.invokeRuntime(
      "retain", nullptr, {ObjectTypeSet::all()}, {valueEncoder.box(target)});
  if (auto *i = dyn_cast<Instruction>(retain.value)) {
    i->setMetadata("memory_management", meta);
  }
}

TypedValue MemoryManagement::dynamicRelease(TypedValue &target) {
  Metadata *metaPtr = dyn_cast<Metadata>(MDString::get(context, "release"));
  MDNode *meta = MDNode::get(context, metaPtr);

  TypedValue release = invoke.invokeRuntime(
      "release", nullptr, {ObjectTypeSet::all()}, {valueEncoder.box(target)});
  if (auto *i = dyn_cast<Instruction>(release.value)) {
    i->setMetadata("memory_management", meta);
  }
  return release;
}

void MemoryManagement::dynamicIsReusable(TypedValue &target) {}

// CleanupChainGuard implementation
CleanupChainGuard::CleanupChainGuard(CodeGen &c) : cg(c) {}
CleanupChainGuard::~CleanupChainGuard() { popAll(); }

void CleanupChainGuard::push(TypedValue val) {
  if (needsProtection(val.type)) {
    cg.pushResource(val);
    pushedCount++;
  }
}

void CleanupChainGuard::popAll() {
  while (pushedCount > 0) {
    cg.popResource();
    pushedCount--;
  }
}

} // namespace rt
