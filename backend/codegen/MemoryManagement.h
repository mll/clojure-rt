#ifndef MEMORY_MANAGEMENT_H
#define MEMORY_MANAGEMENT_H

#include "LLVMTypes.h"
#include "TypedValue.h"
#include "ValueEncoder.h"
#include "VariableBindings.h"
#include "bytecode.pb.h"
#include "invoke/InvokeManager.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

using namespace clojure::rt::protobuf::bytecode;

namespace rt {

class MemoryManagement {
  friend class CleanupChainGuard;

public:
  explicit MemoryManagement(llvm::LLVMContext &context, llvm::IRBuilder<> &b,
                            llvm::Module &m, ValueEncoder &v, LLVMTypes &t,
                            VariableBindings<TypedValue> &vb, InvokeManager &i);

  /// Initializes the exception handling infrastructure for a new LLVM function.
  /// Allocates the exception slot and creates the terminal resume block for
  /// unwinding. This is called directly for flat, independent functions (like
  /// topLevel or inlinecachecall), or internally via
  /// suspendStateForNestedFunction for nested clojurefunctions.
  void initFunction(llvm::Function *F);

  /// Emits LLVM IR to perform memory operations (like retain/release) based on
  /// explicit guidance provided by the AST compiler, typically used to manage
  /// the lifetimes of local bindings during complex control flow.
  void dynamicMemoryGuidance(const MemoryManagementGuidance &guidance);

  /// Emits IR to increment the reference count of a managed value.
  void dynamicRetain(const TypedValue &target);

  /// Emits IR to decrement the reference count of a managed value.
  TypedValue dynamicRelease(const TypedValue &target);

  /// Emits IR to check if a value's reference count is 1, enabling in-place
  /// mutation optimizations (e.g., for Clojure's transients).
  void dynamicIsReusable(const TypedValue &target);

  /// Retrieves or lazily generates a landing pad basic block for catching
  /// exceptions. The landing pad routes execution through the cleanup stack to
  /// release all currently active resources.
  /// @param skipCount Number of top active resources to exclude from cleanup
  /// (useful
  ///                  when the throwing expression shouldn't release its own
  ///                  result).
  /// @param extraCleanup Additional ad-hoc resources to release during this
  /// specific unwind.
  llvm::BasicBlock *
  getLandingPad(size_t skipCount = 0,
                const std::vector<TypedValue> &extraCleanup = {});

  /// Checks if there are any active resources pushed to the current state that
  /// would need to be released in the event of an exception.
  bool hasPushedResources() const;

  /// Checks if there is any active AST-provided unwind guidance (instructions
  /// for manually modifying reference counts during unwind) currently in
  /// effect.
  bool hasActiveGuidance() const;

  /// Resets the memory management state, clearing all resources, caches, and
  /// guidance.
  void clear();

  /// Encapsulates the memory management state for a single function being
  /// compiled. Used to preserve state when pausing compilation of one function
  /// to compile another (e.g., when generating IR for nested closures or
  /// lambdas).
  struct FunctionState {
    llvm::Value *exceptionSlot;
    llvm::BasicBlock *terminalResumeBB;
    std::vector<llvm::BasicBlock *> cleanupStack;
    std::vector<TypedValue> activeResources;
    size_t totalPushedResources;
    size_t resourcesWithCleanup;
    std::map<size_t, llvm::BasicBlock *> lpadCache;
    const google::protobuf::RepeatedPtrField<MemoryManagementGuidance>
        *activeUnwindGuidance;
  };

  /// Saves the current memory management state onto the stateStack and
  /// initializes a fresh state for F by calling initFunction(F). This is used
  /// when pausing the compilation of an outer function to compile a nested
  /// clojurefunction (e.g., a lambda), ensuring the outer function's active
  /// resources and landing pads are preserved.
  void suspendStateForNestedFunction(llvm::Function *F);

  /// Restores the previously saved memory management state from the stateStack.
  /// Called after a nested clojurefunction finishes compilation, allowing the
  /// compiler to resume generating the outer function with its original
  /// resources intact.
  void restoreStateFromNestedFunction();

  /// Returns the currently active AST-provided memory management guidance.
  const google::protobuf::RepeatedPtrField<MemoryManagementGuidance> *
  getActiveUnwindGuidance() const;

  /// Sets the AST-provided memory management guidance to be applied during
  /// unwinding. This also clears the landing pad cache since the generated
  /// landing pads will now need to include the new guidance operations.
  void setActiveUnwindGuidance(
      const google::protobuf::RepeatedPtrField<MemoryManagementGuidance>
          *guidance);

  /// Clears the currently active memory management guidance and invalidates the
  /// landing pad cache.
  void clearActiveUnwindGuidance();

  /// A RAII guard for temporarily applying AST-provided memory management
  /// guidance during the compilation of a specific expression or block,
  /// restoring the previous guidance when it goes out of scope.
  struct UnwindGuidanceGuard {
    MemoryManagement &mm;
    const google::protobuf::RepeatedPtrField<MemoryManagementGuidance> *prev;
    UnwindGuidanceGuard(
        MemoryManagement &mm,
        const google::protobuf::RepeatedPtrField<MemoryManagementGuidance>
            *current);
    ~UnwindGuidanceGuard();
  };

private:
  // Exception safety / Resource management

  /// Registers a dynamically allocated value as an active resource. If an
  /// exception occurs, this resource will be automatically released during
  /// stack unwinding.

  void pushResource(TypedValue val);

  /// Deregisters the most recently pushed resource. Called when a resource's
  /// lifetime ends normally without an exception (e.g., when it is returned or
  /// consumed).

  void popResource();

  llvm::LLVMContext &context;
  llvm::IRBuilder<> &builder;

  ValueEncoder &valueEncoder;
  LLVMTypes &types;
  VariableBindings<TypedValue> &variableBindingStack;
  InvokeManager &invoke;

  // Landing pad / Exception state

  /// An alloca instruction in the function's entry block used to temporarily
  /// store the caught exception pointer during cleanup block execution.
  llvm::Value *exceptionSlot = nullptr;

  /// The final basic block in the unwind path that executes the 'resume'
  /// instruction to continue propagating the exception up the call stack after
  /// all local cleanups.
  llvm::BasicBlock *terminalResumeBB = nullptr;

  /// A stack of basic blocks, where each block releases one or more active
  /// resources and then branches to the next block in the stack or to the
  /// terminalResumeBB.
  std::vector<llvm::BasicBlock *> cleanupStack;

  /// A list of values that are currently alive in the local scope and must be
  /// released if an exception unwinds past the current point.
  std::vector<TypedValue> activeResources;

  /// The total number of resources pushed over the lifetime of the current
  /// function state. Used to uniquely identify the current unwind state for
  /// landing pad caching.
  size_t totalPushedResources = 0;

  /// An index into `activeResources` representing the high-water mark for which
  /// cleanup blocks have already been generated. Prevents redundant cleanup
  /// generation.
  size_t resourcesWithCleanup = 0; // Index into activeResources

  /// A cache mapping an unwind state identifier (based on totalPushedResources
  /// and skipCount) to its corresponding landing pad block. Avoids emitting
  /// identical landing pads multiple times.
  std::map<size_t, llvm::BasicBlock *> lpadCache;

  /// Pointer to the current set of AST-provided unwind instructions, dictating
  /// specific memory bindings to retain or release during exception unwinding.
  const google::protobuf::RepeatedPtrField<MemoryManagementGuidance>
      *activeUnwindGuidance = nullptr;

  /// Stack of saved function states, supporting nested function compilation.
  std::vector<FunctionState> stateStack;
  void *jitEnginePtr = nullptr;

  /// Lazily allocates the exception handling slot (`exceptionSlot`) and creates
  /// the terminal resume block (`terminalResumeBB`) if they haven't been
  /// created yet for the current function. This ensures we don't emit unused
  /// exception blocks in functions that never actually throw or catch.
  void ensureExceptionInfrastructure(llvm::Function *F);
};

} // namespace rt

#endif // VALUE_ENCODER_H
