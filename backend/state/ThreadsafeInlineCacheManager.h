#ifndef INLINE_CACHE_MANAGER_H
#define INLINE_CACHE_MANAGER_H

#include "../RuntimeHeaders.h"
#include "../runtime/word.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace rt {
struct ThreadsafeInlineCacheSlot {
  std::atomic<RTValue> tag{0};

  union Payload {
    std::atomic<uint32_t> offset;
    std::atomic<void *> pointer;
    Payload() : offset(0) {}
  } payload;

  enum class Kind { Field, Call } kind;
};

class ThreadsafeInlineCacheManager {
private:
  std::mutex mutex;
  std::vector<std::unique_ptr<ThreadsafeInlineCacheSlot>> slots;

public:
  ThreadsafeInlineCacheSlot *allocateSlot() {
    std::lock_guard<std::mutex> lock(mutex);
    auto slot = std::make_unique<ThreadsafeInlineCacheSlot>();
    ThreadsafeInlineCacheSlot *ptr = slot.get();
    slots.push_back(std::move(slot));
    return ptr;
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mutex);
    slots.clear();
  }
};

/* Value* emitGenericGuard(IRBuilder<>& builder,  */
/*                         Value* currentTag,  */
/*                         InlineCacheSlot* slot) { */
/*     LLVMContext& ctx = builder.getContext(); */
/*     Value* slotAddr = builder.getInt64(reinterpret_cast<uintptr_t>(slot)); */

/*     // Załaduj tag ze slotu (Atomowo) */
/*     LoadInst* cachedTag = builder.CreateLoad(PointerType::getUnqual(ctx),
 * slotAddr); */
/*     cachedTag->setOrdering(AtomicOrdering::Monotonic); */

/*     // Sprawdź trafienie w cache */
/*     return builder.CreateICmpEQ(currentTag, cachedTag); */
/* } */

/* void generateCallWithIC(IRBuilder<>& builder,  */
/*                         Module* module,  */
/*                         Value* varPtr, // Wskaźnik do globalnego Var-a */
/*                         std::vector<Value*> args) { */
/*     LLVMContext& ctx = builder.getContext(); */
/*     auto* cacheSlot = metadataManager.allocateFunctionSlot(); */
/*     Value* cacheSlotAddr =
 * builder.getInt64(reinterpret_cast<uintptr_t>(cacheSlot)); */

/*     // 1. Pobierz aktualną funkcję z Var (może być redefiniowana) */
/*     Value* currentFnObj = builder.CreateLoad(PointerType::getUnqual(ctx),
 * varPtr, "current_fn_obj"); */

/*     // 2. Pobierz dane z cache */
/*     Value* cachedFnObj = builder.CreateLoad(PointerType::getUnqual(ctx),
 * cacheSlotAddr, "cached_fn_obj"); */

/*     Value* isHit = builder.CreateICmpEQ(currentFnObj, cachedFnObj,
 * "is_call_hit"); */

/*     Function* parentFunc = builder.GetInsertBlock()->getParent(); */
/*     BasicBlock* fastPath = BasicBlock::Create(ctx, "call_fast", parentFunc);
 */
/*     BasicBlock* slowPath = BasicBlock::Create(ctx, "call_slow", parentFunc);
 */
/*     BasicBlock* exitBlock = BasicBlock::Create(ctx, "call_exit", parentFunc);
 */

/*     builder.CreateCondBr(isHit, fastPath, slowPath); */

/*     // --- Fast Path: Bezpośredni skok --- */
/*     builder.SetInsertPoint(fastPath); */
/*     Value* codePtrAddr = builder.CreatePtrAdd(cacheSlotAddr,
 * builder.getInt64(8)); */
/*     Value* cachedCode = builder.CreateLoad(PointerType::getUnqual(ctx),
 * codePtrAddr, "cached_code"); */

/*     // Rzutowanie na sygnaturę funkcji i wywołanie */
/*     FunctionType* fnSig = FunctionType::get(builder.getInt64Ty(), { /\* typy
 * argumentów *\/ }, false); */
/*     Value* resultFast = builder.CreateCall(fnSig, cachedCode, args); */
/*     builder.CreateBr(exitBlock); */

/*     // --- Slow Path: Runtime lookup --- */
/*     builder.SetInsertPoint(slowPath); */
/*     Function* updateFunc = module->getFunction("runtime_update_fn_cache"); */
/*     Value* resultSlow = builder.CreateCall(updateFunc, {currentFnObj,
 * cacheSlotAddr, ...args}); */
/*     builder.CreateBr(exitBlock); */

/*     // ... PHI node dla wyniku ... */
/* } */

// Dostep do structury:

/* extern  "C" int64_t runtime_update_cache(Object* obj, InlineCacheSlot* slot,
 * const char* name) { */
/*     TypeDescriptor* type = obj->header->type; */
/*     int64_t offset = type->layout.findOffset(name); */

/*     // Atomowe zapisy gwarantują, że inne wątki zobaczą albo starą,  */
/*     // albo nową wartość, nigdy nic pomiędzy. */
/*     slot->lastTypeDescriptor.store(type, std::memory_order_relaxed); */
/*     slot->lastOffset.store(offset, std::memory_order_relaxed); */

/*     return *(int64_t*)((char*)obj + offset); */
/* } */

} // namespace rt

#endif
