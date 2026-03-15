#include "MemoryManagement.h"
#include "../RuntimeHeaders.h"
#include "../bridge/Exceptions.h"
#include "../cljassert.h"
#include "../runtime/word.h"
#include "CleanupChainGuard.h"
#include "CodeGen.h"
#include "DynamicConstructor.h"
#include "LLVMTypes.h"
#include "TypedValue.h"
#include "ValueEncoder.h"
#include "VariableBindings.h"
#include "runtime/ObjectProto.h"
#include "types/ObjectTypeSet.h"
#include "llvm/ADT/StringSwitch.h"
#include <cstdint>
#include <llvm/IR/Constants.h>
#include <string>

using namespace llvm;

namespace rt {

MemoryManagement::MemoryManagement(llvm::LLVMContext &c, IRBuilder<> &b,
                                   llvm::Module &m, ValueEncoder &v,
                                   LLVMTypes &t,
                                   VariableBindings<TypedValue> &vb,
                                   InvokeManager &i)
    : context(c), builder(b), valueEncoder(v), types(t),
      variableBindingStack(vb), invoke(i) {}

void MemoryManagement::initFunction(llvm::Function *F) {
  exceptionSlot = nullptr;
  terminalResumeBB = nullptr;
  clear();
}

void MemoryManagement::ensureExceptionInfrastructure(llvm::Function *F) {
  if (exceptionSlot)
    return;

  auto currentIP = builder.saveIP();

  // Create alloca in entry block if possible, or just here
  exceptionSlot = builder.CreateAlloca(
      llvm::StructType::get(context, {types.ptrTy, types.i32Ty}), nullptr,
      "exception_slot");
  terminalResumeBB = llvm::BasicBlock::Create(context, "terminal_resume", F);

  builder.SetInsertPoint(terminalResumeBB);
  llvm::Value *exVal = builder.CreateLoad(
      llvm::StructType::get(context, {types.ptrTy, types.i32Ty}), exceptionSlot,
      "exception_to_resume");
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
  if (lpadCache.count(skipCount)) {
    return lpadCache[skipCount];
  }

  // 1. Calculate how many resources effectively need protection (survivors)
  size_t survivorCount = (skipCount < activeResources.size())
                             ? (activeResources.size() - skipCount)
                             : 0;

  bool hasGuidance = activeUnwindGuidance && !activeUnwindGuidance->empty();

  size_t neededSurvivors = 0;
  for (size_t i = 0; i < survivorCount; ++i) {
    if (CleanupChainGuard::needsProtection(activeResources[i].type)) {
      neededSurvivors++;
    }
  }

  if (neededSurvivors == 0 && !hasGuidance) {
    return nullptr;
  }

  // 3. Ensure infrastructure
  IRBuilder<>::InsertPointGuard guard(builder);
  Function *F = builder.GetInsertBlock()->getParent();
  ensureExceptionInfrastructure(F);

  // 4. Fill cleanup stack (Lazily)
  while (resourcesWithCleanup < survivorCount) {
    TypedValue val = activeResources[resourcesWithCleanup];
    if (CleanupChainGuard::needsProtection(val.type)) {
      llvm::BasicBlock *prevCleanup =
          cleanupStack.empty() ? terminalResumeBB : cleanupStack.back();

      llvm::BasicBlock *newCleanup = llvm::BasicBlock::Create(
          context,
          "cleanup_link_" +
              std::to_string(totalPushedResources - (activeResources.size() -
                                                     resourcesWithCleanup - 1)),
          F);

      builder.SetInsertPoint(newCleanup);
      dynamicRelease(val);
      builder.CreateBr(prevCleanup);
      cleanupStack.push_back(newCleanup);
    }
    resourcesWithCleanup++;
  }

  // 5. Create Landing Pad entry
  auto *lpad =
      BasicBlock::Create(context, "lpad_entry_" + std::to_string(skipCount), F);
  builder.SetInsertPoint(lpad);
  LandingPadInst *lp = builder.CreateLandingPad(
      llvm::StructType::get(context, {types.ptrTy, types.i32Ty}), 1);
  lp->setCleanup(true);
  builder.CreateStore(lp, exceptionSlot);

  // 6. Execute Unwind Guidance
  if (hasGuidance) {
    for (const auto &g : *activeUnwindGuidance) {
      auto name = g.variablename();
      auto change = g.requiredrefcountchange();

      for (word_t depth = (word_t)variableBindingStack.stackDepth() - 1;
           depth >= 0; depth--) {
        auto val = variableBindingStack.find(name, depth);
        if (val) {
          int count = std::abs(change);
          while (count > 0) {
            if (change > 0)
              dynamicRetain(*val);
            else
              dynamicRelease(*val);
            count--;
          }
          break;
        }
      }
    }
  }

  // 7. Branch to cleanup stack or resume
  if (neededSurvivors > 0) {
    builder.CreateBr(cleanupStack[neededSurvivors - 1]);
  } else {
    builder.CreateBr(terminalResumeBB);
  }

  lpadCache[skipCount] = lpad;
  return lpad;
}

void MemoryManagement::clear() {
  cleanupStack.clear();
  activeResources.clear();
  lpadCache.clear();
  totalPushedResources = 0;
  resourcesWithCleanup = 0;
  activeUnwindGuidance = nullptr;
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
  if (target.type.isScalar() || target.type.isBoxedScalar())
    return;
  Metadata *metaPtr = dyn_cast<Metadata>(MDString::get(context, "retain"));
  MDNode *meta = MDNode::get(context, metaPtr);

  TypedValue retain = invoke.invokeRuntime(
      "retain", nullptr, {ObjectTypeSet::all()}, {valueEncoder.box(target)});
  if (auto *i = dyn_cast<Instruction>(retain.value)) {
    i->setMetadata("memory_management", meta);
  }
}

TypedValue MemoryManagement::dynamicRelease(TypedValue &target) {
  if (target.type.isScalar() || target.type.isBoxedScalar())
    return TypedValue(ObjectTypeSet(booleanType),
                      ConstantInt::get(context, APInt(1, 0)));
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
