#ifndef CODEGEN_H
#define CODEGEN_H

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>        
#include <llvm/IR/Verifier.h>      
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include "TypedValue.h"
#include "ValueEncoder.h"
#include "../state/ThreadsafeState.h"
#include "../bytecode.pb.h"
#include "LLVMTypes.h"
#include "DynamicConstructor.h"
#include "VariableBindings.h"
#include "invoke/InvokeManager.h"
#include "MemoryManagement.h"

using namespace clojure::rt::protobuf::bytecode;

namespace rt {
  class CodeGen {   
    llvm::orc::ThreadSafeContext TSContext;
    
    llvm::LLVMContext &TheContext;
    std::unique_ptr<llvm::Module> TheModule;    
    llvm::IRBuilder<> Builder;

    VariableBindings<TypedValue> variableBindingStack;
    VariableBindings<ObjectTypeSet> variableTypesBindingsStack;    
    
    LLVMTypes types;
    ValueEncoder valueEncoder;
    InvokeManager invokeManager;
    DynamicConstructor dynamicConstructor;
    MemoryManagement memoryManagement;    
    
    std::shared_ptr<ThreadsafeCompilerState> compilerState;
  public:
    CodeGen(std::string_view ModuleName,
            std::shared_ptr<ThreadsafeCompilerState> state)
        : TSContext(std::make_unique<llvm::LLVMContext>()),
          TheContext(*TSContext.getContext()),
          TheModule(std::make_unique<llvm::Module>(ModuleName, TheContext)),
          Builder(TheContext), types(TheContext),
          valueEncoder(TheContext, Builder, types),
          invokeManager(Builder, *TheModule, valueEncoder, types),
          dynamicConstructor(types, invokeManager),
          memoryManagement(TheContext, Builder, valueEncoder, types,
                           variableBindingStack, invokeManager,
                           dynamicConstructor),          
          compilerState(std::move(state))          
    {}

    TypedValue codegen(const Node &node, const ObjectTypeSet &typeRestrictions);

    TypedValue codegen(const Node &node, const ConstNode &subnode, const ObjectTypeSet &typeRestrictions);    

    
    // TypedValue codegen(const Node &node, const BindingNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const CaseNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const CaseTestNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const CaseThenNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const CatchNode &subnode, const ObjectTypeSet &typeRestrictions);

    // TypedValue codegen(const Node &node, const DefNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const DeftypeNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const DoNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const FnNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const FnMethodNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const HostInteropNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const IfNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const ImportNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const InstanceCallNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const InstanceFieldNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const IsInstanceNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const InvokeNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const KeywordInvokeNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const LetNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const LetfnNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const LocalNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const LoopNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const MapNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const MethodNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const MonitorEnterNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const MonitorExitNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const NewNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const PrimInvokeNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const ProtocolInvokeNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const QuoteNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const RecurNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const ReifyNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const SetNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const MutateSetNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const StaticCallNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const StaticFieldNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const TheVarNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const ThrowNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const TryNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const VarNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const VectorNode &subnode, const ObjectTypeSet &typeRestrictions);
    // TypedValue codegen(const Node &node, const WithMetaNode &subnode, const ObjectTypeSet &typeRestrictions);
    
    ObjectTypeSet getType(const Node &node, const ObjectTypeSet &typeRestrictions);

    ObjectTypeSet getType(const Node &node, const ConstNode &subnode, const ObjectTypeSet &typeRestrictions);
    
    // ObjectTypeSet getType(const Node &node, const BindingNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const CaseNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const CaseTestNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const CaseThenNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const CatchNode &subnode, const ObjectTypeSet &typeRestrictions);

    // ObjectTypeSet getType(const Node &node, const DefNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const DeftypeNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const DoNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const FnNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const FnMethodNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const HostInteropNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const IfNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const ImportNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const InstanceCallNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const InstanceFieldNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const IsInstanceNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const InvokeNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const KeywordInvokeNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const LetNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const LetfnNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const LocalNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const LoopNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const MapNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const MethodNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const MonitorEnterNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const MonitorExitNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const NewNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const PrimInvokeNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const ProtocolInvokeNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const QuoteNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const RecurNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const ReifyNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const SetNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const MutateSetNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const StaticCallNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const StaticFieldNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const TheVarNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const ThrowNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const TryNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const VarNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const VectorNode &subnode, const ObjectTypeSet &typeRestrictions);
    // ObjectTypeSet getType(const Node &node, const WithMetaNode &subnode, const ObjectTypeSet &typeRestrictions);    
    
  };
} // namespace rt
  
#endif
