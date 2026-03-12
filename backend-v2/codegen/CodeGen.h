#ifndef CODEGEN_H
#define CODEGEN_H

#include "../state/ThreadsafeCompilerState.h"
#include "DynamicConstructor.h"
#include "LLVMTypes.h"
#include "MemoryManagement.h"
#include "TypedValue.h"
#include "ValueEncoder.h"
#include "VariableBindings.h"
#include "bytecode.pb.h"
#include "invoke/InvokeManager.h"
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

public:
  CodeGen(std::string_view ModuleName, ThreadsafeCompilerState &state)
      : TSContext(std::make_unique<llvm::orc::ThreadSafeContext>(
            std::make_unique<llvm::LLVMContext>())),
        TheContext(*(TSContext->getContext())),
        TheModule(std::make_unique<llvm::Module>(ModuleName, TheContext)),
        Builder(TheContext), types(TheContext),
        valueEncoder(TheContext, Builder, types),
        invokeManager(Builder, *TheModule, valueEncoder, types, state),
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
};
} // namespace rt

#endif
