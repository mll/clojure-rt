#ifndef CODEGEN_H
#define CODEGEN_H

#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <memory>
#include <stdexcept>
#include <string>

#include "TypedValue.h"
#include "LLVMTypes.h"

namespace rt {
class CodeGen;

class ShadowStackGuard {
  CodeGen &cg;
  size_t pushedCount = 0;

public:
  explicit ShadowStackGuard(CodeGen &c) : cg(c) {}
  ~ShadowStackGuard();

  void push(TypedValue val);
  void popAll();

  size_t size() const { return pushedCount; }
  
  // Static helper to check if a type needs shadow stack protection
  static bool needsProtection(const ObjectTypeSet &type) {
    return !type.isScalar() && !type.isBoxedScalar();
  }

  // Prevent copying
  ShadowStackGuard(const ShadowStackGuard &) = delete;
  ShadowStackGuard &operator=(const ShadowStackGuard &) = delete;
};
} // namespace rt

#include "../state/ThreadsafeCompilerState.h"
#include "DynamicConstructor.h"
#include "MemoryManagement.h"
#include "ValueEncoder.h"
#include "VariableBindings.h"
#include "bytecode.pb.h"
#include "invoke/InvokeManager.h"

using namespace clojure::rt::protobuf::bytecode;

namespace rt {

struct CodeGenResult {
  std::unique_ptr<llvm::orc::ThreadSafeContext> context;
  std::unique_ptr<llvm::Module> module;
  std::vector<RTValue> constants;
};

class CodeGen {
  std::unique_ptr<llvm::orc::ThreadSafeContext> TSContext;

  llvm::LLVMContext &TheContext;
  std::unique_ptr<llvm::Module> TheModule;
  llvm::IRBuilder<> Builder;

  VariableBindings<TypedValue> variableBindingStack;
  VariableBindings<ObjectTypeSet> variableTypesBindingsStack;

  LLVMTypes types;
  ValueEncoder valueEncoder;
  InvokeManager invokeManager;
  std::vector<RTValue> generatedConstants;
  DynamicConstructor dynamicConstructor;
  MemoryManagement memoryManagement;

  ThreadsafeCompilerState &compilerState;
  std::unique_ptr<llvm::DIBuilder> DIB;
  llvm::DICompileUnit *CU;
  std::vector<llvm::DIScope *> LexicalBlocks;
  
  // Shadow stack for exception safety
  llvm::Value *shadowStack = nullptr;
  llvm::Value *shadowStackPtr = nullptr;
  llvm::BasicBlock *globalLandingPad = nullptr;
  llvm::FunctionCallee personalityFn;
  size_t totalPushed = 0;
  static constexpr size_t SHADOW_STACK_SIZE = 128; // Per-function limit

public:
  CodeGen(std::string_view ModuleName, ThreadsafeCompilerState &state)
      : TSContext(std::make_unique<llvm::orc::ThreadSafeContext>(
            std::make_unique<llvm::LLVMContext>())),
        TheContext(*(TSContext->getContext())),
        TheModule(std::make_unique<llvm::Module>(ModuleName, TheContext)),
        Builder(TheContext), types(TheContext),
        valueEncoder(TheContext, Builder, types),
        invokeManager(Builder, *TheModule, valueEncoder, types, state, *this),
        dynamicConstructor(types, invokeManager, generatedConstants),
        memoryManagement(TheContext, Builder, valueEncoder, types,
                         variableBindingStack, invokeManager,
                         dynamicConstructor),
        compilerState(state) {
    TheModule->addModuleFlag(llvm::Module::Warning, "Debug Info Version",
                             llvm::DEBUG_METADATA_VERSION);
#ifdef __APPLE__
    TheModule->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 4);
#endif
    DIB = std::make_unique<llvm::DIBuilder>(*TheModule);
    CU = DIB->createCompileUnit(llvm::dwarf::DW_LANG_C,
                                DIB->createFile(ModuleName, "."), "clojure-rt",
                                false, "", 0);
  }

  ~CodeGen();

  CodeGenResult release() &&;

  std::string codegenTopLevel(const Node &node);

  TypedValue codegen(const Node &node, const ObjectTypeSet &typeRestrictions);

  TypedValue codegen(const Node &node, const ConstNode &subnode,
                     const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const QuoteNode &subnode,
                     const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const MapNode &subnode,
                     const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const VectorNode &subnode,
                     const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const StaticCallNode &subnode,
                     const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const TheVarNode &subnode,
                     const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const IfNode &subnode,
                     const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const VarNode &subnode,
                     const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const DefNode &subnode,
                     const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const WithMetaNode &subnode,
                     const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const StaticFieldNode &subnode,
                     const ObjectTypeSet &typeRestrictions);

  ObjectTypeSet getType(const Node &node,
                        const ObjectTypeSet &typeRestrictions);

  ObjectTypeSet getType(const Node &node, const TheVarNode &subnode,
                        const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const IfNode &subnode,
                        const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const ConstNode &subnode,
                        const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const QuoteNode &subnode,
                        const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const VectorNode &subnode,
                        const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const MapNode &subnode,
                        const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const StaticCallNode &subnode,
                        const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const VarNode &subnode,
                        const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const DefNode &subnode,
                        const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const WithMetaNode &subnode,
                        const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const StaticFieldNode &subnode,
                        const ObjectTypeSet &typeRestrictions);
  Var *getOrCreateVar(std::string_view name);
  bool canThrow(const clojure::rt::protobuf::bytecode::Node &node);
  
  // Exception safety helpers
  void pushResource(TypedValue val);
  void popResource();
  void generateLandingPad();
  llvm::BasicBlock* getLandingPad();
  bool hasLandingPad() const { return globalLandingPad != nullptr; }
  bool hasPushedResources() const { return totalPushed > 0; }
};

inline ShadowStackGuard::~ShadowStackGuard() {
  for (size_t i = 0; i < pushedCount; i++) {
    cg.popResource();
  }
}

inline void ShadowStackGuard::push(TypedValue val) {
  if (!needsProtection(val.type)) return;
  cg.pushResource(val);
  pushedCount++;
}

inline void ShadowStackGuard::popAll() {
  for (size_t i = 0; i < pushedCount; i++) {
    cg.popResource();
  }
  pushedCount = 0;
}

} // namespace rt

#endif
