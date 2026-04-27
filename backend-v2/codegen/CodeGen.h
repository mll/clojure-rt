#ifndef CODEGEN_H
#define CODEGEN_H

#include "../bridge/SourceLocation.h"
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "TypedValue.h"

#include "CleanupChainGuard.h"

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
  std::vector<std::string> icSlotNames;
  std::map<SourceLocation, std::string> formMap;
  std::map<BaseSourceLocation, std::string> contextFormMap;
};

struct FunctionMetrics {
  int nodeCount = 0;
  bool hasInvoke = false;
};

struct RecurTarget {
  llvm::Function *function;
  llvm::Value *framePtr;
  int numParams;
  bool isVariadic;
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

  // Static Landing Pads management via manager
  llvm::FunctionCallee personalityFn;

  std::vector<FunctionMetrics> functionMetricsStack;
  std::map<std::string, RecurTarget> recurTargets;

public:
  void *jitEnginePtr;
  CodeGen(std::string_view ModuleName, ThreadsafeCompilerState &state,
          void *jitEngine = nullptr)
      : TSContext(std::make_unique<llvm::orc::ThreadSafeContext>(
            std::make_unique<llvm::LLVMContext>())),
        TheContext(*(TSContext->getContext())),
        TheModule(std::make_unique<llvm::Module>(ModuleName, TheContext)),
        Builder(TheContext), types(TheContext),
        valueEncoder(TheContext, Builder, types),
        invokeManager(Builder, *TheModule, valueEncoder, types, state, *this),
        dynamicConstructor(types, invokeManager, generatedConstants),
        memoryManagement(TheContext, Builder, *TheModule, valueEncoder, types,
                         variableBindingStack, invokeManager),
        compilerState(state), jitEnginePtr(jitEngine) {
    TheModule->addModuleFlag(llvm::Module::Warning, "Debug Info Version",
                             llvm::DEBUG_METADATA_VERSION);
#ifdef __APPLE__
    TheModule->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 5);
#endif
    DIB = std::make_unique<llvm::DIBuilder>(*TheModule);
    CU = DIB->createCompileUnit(llvm::dwarf::DW_LANG_C,
                                DIB->createFile(ModuleName, "."), "clojure-rt",
                                false, "", 0);
  }

  ~CodeGen();

  CodeGenResult release() &&;

  const std::map<SourceLocation, std::string> &getFormMap() const {
    return formMap;
  }

  const std::map<BaseSourceLocation, std::string> &getContextFormMap() const {
    return contextFormMap;
  }

  std::string codegenTopLevel(const Node &node);
  std::string
  generateInstanceCallBridge(const std::string &moduleName,
                             const std::string &methodName,
                             const ObjectTypeSet &instanceType,
                             const std::vector<ObjectTypeSet> &argTypes);

  llvm::Function *generateBaselineMethod(
      const FnMethodNode &method,
      const std::vector<std::pair<std::string, ObjectTypeSet>> &captureInfo);

  TypedValue codegen(const Node &node, const ObjectTypeSet &typeRestrictions);

  TypedValue codegen(const Node &node, const ConstNode &subnode,
                     const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const QuoteNode &subnode,
                     const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const DoNode &subnode,
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
  TypedValue codegen(const Node &node, const FnNode &subnode,
                     const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const StaticFieldNode &subnode,
                     const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const LetNode &subnode,
                     const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const LocalNode &subnode,
                     const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const InstanceCallNode &subnode,
                     const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const HostInteropNode &subnode,
                     const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const NewNode &subnode,
                     const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const IsInstanceNode &subnode,
                     const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const ThrowNode &subnode,
                     const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const InvokeNode &subnode,
                     const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const KeywordInvokeNode &subnode,
                     const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const RecurNode &subnode,
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
  ObjectTypeSet getType(const Node &node, const DoNode &subnode,
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
  ObjectTypeSet getType(const Node &node, const FnNode &subnode,
                        const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const StaticFieldNode &subnode,
                        const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const LetNode &subnode,
                        const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const LocalNode &subnode,
                        const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const InstanceCallNode &subnode,
                        const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const HostInteropNode &subnode,
                        const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const NewNode &subnode,
                        const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const IsInstanceNode &subnode,
                        const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const ThrowNode &subnode,
                        const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const InvokeNode &subnode,
                        const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const KeywordInvokeNode &subnode,
                        const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const RecurNode &subnode,
                        const ObjectTypeSet &typeRestrictions);

  Var *getOrCreateVar(std::string_view name);
  bool canThrow(const clojure::rt::protobuf::bytecode::Node &node);

  // Exception safety helpers
  void pushResource(TypedValue val) { memoryManagement.pushResource(val); }
  void popResource() { memoryManagement.popResource(); }
  llvm::BasicBlock *getLandingPad(size_t skipCount = 0) {
    return memoryManagement.getLandingPad(skipCount);
  }
  bool hasLandingPad() const {
    return memoryManagement.hasPushedResources() ||
           memoryManagement.hasActiveGuidance();
  }
  bool hasPushedResources() const {
    return memoryManagement.hasPushedResources();
  }
  MemoryManagement &getMemoryManagement() { return memoryManagement; }
  DynamicConstructor &getDynamicConstructor() { return dynamicConstructor; }

  class FunctionScopeGuard {
    CodeGen &cg;
    llvm::IRBuilder<>::InsertPointGuard ipGuard;
    size_t prevLexicalBlocksSize;

  public:
    FunctionScopeGuard(CodeGen &cg, llvm::Function *newFunction)
        : cg(cg), ipGuard(cg.Builder),
          prevLexicalBlocksSize(cg.LexicalBlocks.size()) {
      cg.memoryManagement.pushState(newFunction);
    }
    ~FunctionScopeGuard() {
      cg.memoryManagement.popState();
      while (cg.LexicalBlocks.size() > prevLexicalBlocksSize) {
        cg.LexicalBlocks.pop_back();
      }
    }
  };

  class DebugLocGuard {
    llvm::IRBuilder<> &Builder;
    llvm::DebugLoc savedLoc;

  public:
    DebugLocGuard(llvm::IRBuilder<> &Builder)
        : Builder(Builder), savedLoc(Builder.getCurrentDebugLocation()) {}
    ~DebugLocGuard() { Builder.SetCurrentDebugLocation(savedLoc); }
  };

private:
  std::map<SourceLocation, std::string> formMap;
  std::map<BaseSourceLocation, std::string> contextFormMap;
  int nodeIDCounter = 0;

public:
  std::string suggestedFunctionName;
  bool isSuggestedNameFromDef = false;
};

} // namespace rt

#endif
