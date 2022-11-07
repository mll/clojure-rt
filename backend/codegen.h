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
#include "ObjectTypeSet.h"
#include "jit.h"
#include "runtime/defines.h"

using namespace std;
using namespace llvm;
using namespace llvm::orc;
using namespace clojure::rt::protobuf::bytecode;
class CodeGenerator;


typedef pair<ObjectTypeSet, Value *> TypedValue;
typedef pair<ObjectTypeSet, const Node&> TypedNode;
typedef TypedValue (*StaticCall)(CodeGenerator *, const string &, const Node&, const std::vector<TypedNode>&);

class CodeGenerationException: public exception {
  string errorMessage;
  public:
  CodeGenerationException(const string &errorMessage, const Node& node); 
  string toString() const; 
};

class InternalInconsistencyException: public exception {
  string errorMessage;
  public:
  InternalInconsistencyException(const string &error) : errorMessage(error) {} 
  string toString() const { return errorMessage; } 
};

class CodeGenerator {
  public:
  std::unique_ptr<LLVMContext> TheContext;
  std::unique_ptr<Module> TheModule;
  std::unique_ptr<IRBuilder<>> Builder;
  unordered_map<string, vector<pair<string, pair<ObjectTypeSet, StaticCall>>>> StaticCallLibrary; 
  std::unordered_map<std::string, TypedValue> NamedValues;
  /* Assumes the Node has FnNode */
  std::unordered_map<std::string, Node> Functions;
  std::unordered_map<std::string, TypedValue> StaticVars;
  std::unique_ptr<legacy::FunctionPassManager> TheFPM;
  std::unique_ptr<ClojureJIT> TheJIT;
  uint64_t lastFunctionUniqueId = 0;
//  std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
  ExitOnError ExitOnErr;

  CodeGenerator();

  /* Tools */

  string typeStringForArgs(const vector<ObjectTypeSet> &args);
  vector<ObjectTypeSet> typesForArgString(const Node &node, const string &typeString); 
  ObjectTypeSet typeForArgString(const Node &node, const string &typeString);
  uint64_t computeHash(const char *str);
  uint64_t avalanche_64(uint64_t h);
  string globalNameForVar(string var);
  string getMangledUniqueFunctionName();

  TypedValue staticFalse();
  TypedValue staticTrue();
  /* Runtime interop */

  Value *dynamicNil();
  Value *dynamicString(const char *str);
  Value *dynamicSymbol(const char *name);
  Value *dynamicKeyword(const char *name);
  Value *dynamicCond(Value *cond);
  Value *box(const TypedValue &value);
  void runtimeException(const CodeGenerationException &runtimeException);  

  Value *callRuntimeFun(const string &fname, Type *retValType, const vector<Type *> &argTypes, const vector<Value *> &args);
  Value *dynamicCreate(objectType type, const vector<Type *> &argTypes, const vector<Value *> &args);
 
  Type *dynamicUnboxedType(objectType type);
  Type *dynamicBoxedType(objectType type);
  Type *dynamicBoxedType();


  /* Code generation */

  int codegen(const Programme &programme);
  TypedValue codegen(const Node &node, const ObjectTypeSet &typeRestrictions);

  TypedValue codegen(const Node &node, const BindingNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const CaseNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const CaseTestNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const CaseThenNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const CatchNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const ConstNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const DefNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const DeftypeNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const DoNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const FnNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const FnMethodNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const HostInteropNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const IfNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const ImportNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const InstanceCallNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const InstanceFieldNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const IsInstanceNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const InvokeNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const KeywordInvokeNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const LetNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const LetfnNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const LocalNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const LoopNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const MapNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const MethodNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const MonitorEnterNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const MonitorExitNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const NewNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const PrimInvokeNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const ProtocolInvokeNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const QuoteNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const RecurNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const ReifyNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const SetNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const MutateSetNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const StaticCallNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const StaticFieldNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const TheVarNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const ThrowNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const TryNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const VarNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const VectorNode &subnode, const ObjectTypeSet &typeRestrictions);
  TypedValue codegen(const Node &node, const WithMetaNode &subnode, const ObjectTypeSet &typeRestrictions);


  ObjectTypeSet getType(const Node &node, const ObjectTypeSet &typeRestrictions);

  ObjectTypeSet getType(const Node &node, const BindingNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const CaseNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const CaseTestNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const CaseThenNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const CatchNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const ConstNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const DefNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const DeftypeNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const DoNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const FnNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const FnMethodNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const HostInteropNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const IfNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const ImportNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const InstanceCallNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const InstanceFieldNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const IsInstanceNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const InvokeNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const KeywordInvokeNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const LetNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const LetfnNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const LocalNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const LoopNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const MapNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const MethodNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const MonitorEnterNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const MonitorExitNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const NewNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const PrimInvokeNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const ProtocolInvokeNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const QuoteNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const RecurNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const ReifyNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const SetNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const MutateSetNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const StaticCallNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const StaticFieldNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const TheVarNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const ThrowNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const TryNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const VarNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const VectorNode &subnode, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet getType(const Node &node, const WithMetaNode &subnode, const ObjectTypeSet &typeRestrictions);



};

#endif
