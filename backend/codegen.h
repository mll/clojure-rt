#ifndef RT_CODEGEN
#define RT_CODEGEN

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include "protobuf/bytecode.pb.h"
#include <fstream>
#include <sstream>
#include "jit.h"

using namespace std;
using namespace llvm;
using namespace llvm::orc;
using namespace clojure::rt::protobuf::bytecode;

class CodeGenerationException: public exception {
  string errorMessage;
  public:
  CodeGenerationException(const string errorMessage, const Node& node); 
  string toString(); 
};


class CodeGenerator {
  public:
  std::unique_ptr<LLVMContext> TheContext;
  std::unique_ptr<Module> TheModule;
  std::unique_ptr<IRBuilder<>> Builder;
  std::map<std::string, Value *> NamedValues;
  std::unique_ptr<legacy::FunctionPassManager> TheFPM;
  std::unique_ptr<ClojureJIT> TheJIT;
//  std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
  ExitOnError ExitOnErr;

  CodeGenerator();
  Value *codegen(const Programme &programme);
  Value *codegen(const Node &node);

  Value *codegen(const Node &node, const BindingNode &subnode);
  Value *codegen(const Node &node, const CaseNode &subnode);
  Value *codegen(const Node &node, const CaseTestNode &subnode);
  Value *codegen(const Node &node, const CaseThenNode &subnode);
  Value *codegen(const Node &node, const CatchNode &subnode);
  Value *codegen(const Node &node, const ConstNode &subnode);
  Value *codegen(const Node &node, const DefNode &subnode);
  Value *codegen(const Node &node, const DeftypeNode &subnode);
  Value *codegen(const Node &node, const DoNode &subnode);
  Value *codegen(const Node &node, const FnNode &subnode);
  Value *codegen(const Node &node, const FnMethodNode &subnode);
  Value *codegen(const Node &node, const HostInteropNode &subnode);
  Value *codegen(const Node &node, const IfNode &subnode);
  Value *codegen(const Node &node, const ImportNode &subnode);
  Value *codegen(const Node &node, const InstanceCallNode &subnode);
  Value *codegen(const Node &node, const InstanceFieldNode &subnode);
  Value *codegen(const Node &node, const IsInstanceNode &subnode);
  Value *codegen(const Node &node, const InvokeNode &subnode);
  Value *codegen(const Node &node, const KeywordInvokeNode &subnode);
  Value *codegen(const Node &node, const LetNode &subnode);
  Value *codegen(const Node &node, const LetfnNode &subnode);
  Value *codegen(const Node &node, const LocalNode &subnode);
  Value *codegen(const Node &node, const LoopNode &subnode);
  Value *codegen(const Node &node, const MapNode &subnode);
  Value *codegen(const Node &node, const MethodNode &subnode);
  Value *codegen(const Node &node, const MonitorEnterNode &subnode);
  Value *codegen(const Node &node, const MonitorExitNode &subnode);
  Value *codegen(const Node &node, const NewNode &subnode);
  Value *codegen(const Node &node, const PrimInvokeNode &subnode);
  Value *codegen(const Node &node, const ProtocolInvokeNode &subnode);
  Value *codegen(const Node &node, const QuoteNode &subnode);
  Value *codegen(const Node &node, const RecurNode &subnode);
  Value *codegen(const Node &node, const ReifyNode &subnode);
  Value *codegen(const Node &node, const SetNode &subnode);
  Value *codegen(const Node &node, const MutateSetNode &subnode);
  Value *codegen(const Node &node, const StaticCallNode &subnode);
  Value *codegen(const Node &node, const StaticFieldNode &subnode);
  Value *codegen(const Node &node, const TheVarNode &subnode);
  Value *codegen(const Node &node, const ThrowNode &subnode);
  Value *codegen(const Node &node, const TryNode &subnode);
  Value *codegen(const Node &node, const VarNode &subnode);
  Value *codegen(const Node &node, const VectorNode &subnode);
  Value *codegen(const Node &node, const WithMetaNode &subnode);
};

#endif
