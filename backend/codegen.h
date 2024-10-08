#ifndef RT_CODEGEN
#define RT_CODEGEN

#include "bytecode.pb.h"
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

#include <fstream>
#include <sstream>

#include "ObjectTypeSet.h"
#include "runtime/defines.h"
#include "Exceptions.h"
#include "Programme.h"
#include "jit/jit.h"
#include "FunctionJIT.h"

extern "C" {
  typedef struct Class Class;
}

#define RUNTIME_MEMORY_TRACKING

using namespace clojure::rt::protobuf::bytecode;

#define REPORT_EX(runtime, EX) ((runtime) ? runtimeException(EX) : (throw (EX)))

class CodeGenerator {
  std::shared_ptr<ProgrammeState> TheProgramme;

  std::vector<std::unordered_map<std::string, TypedValue>> VariableBindingStack;
  std::vector<std::unordered_map<std::string, ObjectTypeSet>> VariableBindingTypesStack;
  std::unordered_map<std::string, std::pair<llvm::BasicBlock *, std::vector<llvm::PHINode *>>> LoopInsertPoints;

public:
  std::unique_ptr<llvm::IRBuilder<>> Builder;
  std::unique_ptr<llvm::LLVMContext> TheContext;
  std::unique_ptr<llvm::Module> TheModule;
  ClojureJIT *TheJIT;
  llvm::ExitOnError ExitOnErr;

  CodeGenerator(std::shared_ptr<ProgrammeState> programme, ClojureJIT *weakJIT);

  /* Tools */

  void printDebugValue(llvm::Value *v);
  uint64_t getUniqueFunctionIdFromName(std::string name);
  std::string pointerName(void *ptr);

  std::vector<ObjectTypeSet> typesForArgString(const Node &node, const std::string &typeString); 
  ObjectTypeSet typeForArgString(const Node &node, const std::string &typeString);
  uint64_t computeHash(const char *str);
  uint64_t avalanche_64(uint64_t h);
  static std::string globalNameForVar(std::string var);
  static std::string getMangledUniqueFunctionName(uint64_t num) ;
  uint64_t getUniqueFunctionId();
  uint64_t getUniqueClassId();

  TypedValue staticFalse();
  TypedValue staticTrue();

/* Dynamic dispatch */


TypedValue callStaticFun(const Node &node, const FnNode& body, const std::pair<FnMethodNode, uint64_t> &method, const std::string &name, const ObjectTypeSet &retValType, const std::vector<TypedValue> &args, const std::string &refName, const TypedValue &callObject, std::vector<ObjectTypeSet> &closedOverTypes);

  void buildStaticFun(const int64_t uniqueId, 
                      const uint64_t methodIndex, 
                      const std::string &name, 
                      const ObjectTypeSet &retVal, 
                      const std::vector<ObjectTypeSet> &args, 
                      std::vector<ObjectTypeSet> &closedOvers);

  llvm::Value *callDynamicFun(const Node &node, llvm::Value *rtFnPointer, const ObjectTypeSet &retValType, const std::vector<TypedValue> &args);
  llvm::Value *dynamicInvoke(const Node &node, llvm::Value *objectToInvoke, llvm::Value* objectType, const ObjectTypeSet &retValType, const std::vector<TypedValue> &args, llvm::Value *uniqueFunctionId = nullptr, llvm::Function *staticFunctionToCall = nullptr);    
  ObjectTypeSet determineMethodReturn(const FnMethodNode &method, const uint64_t uniqueId, const std::vector<ObjectTypeSet> &args, const std::vector<ObjectTypeSet> &closedOvers, const ObjectTypeSet &typeRestrictions);
  ObjectTypeSet inferMethodReturn(const FnMethodNode &method, const uint64_t uniqueId, const std::vector<ObjectTypeSet> &args, const std::vector<ObjectTypeSet> &closedOvers, const ObjectTypeSet &typeRestrictions);

  /* Runtime types */

  llvm::StructType *runtimeObjectType();
  llvm::StructType *runtimeFunctionType(); 
  llvm::StructType *runtimeBooleanType();
  llvm::StructType *runtimeIntegerType();
  llvm::StructType *runtimeDoubleType();
  llvm::StructType *runtimeInvokationCacheType();
  llvm::StructType *runtimeFunctionMethodType();
  llvm::StructType *runtimeClassType();

  llvm::Value *getRuntimeObjectType(llvm::Value *objectPtr);

  /* Runtime interop */

  llvm::Value *dynamicNil();
  llvm::Value *dynamicString(const char *str);
  llvm::Value *dynamicBigInteger(const char *value);
  llvm::Value *dynamicRatio(const char *value);

  llvm::Value *dynamicSymbol(const char *name);
  llvm::Value *dynamicKeyword(const char *name);
  llvm::Value *dynamicVector(const std::vector<TypedValue> &args);
  llvm::Value *dynamicArrayMap(const std::vector<TypedValue> &args);
  void codegenDynamicMemoryGuidance(const Node &node);
  void dynamicMemoryGuidance(const MemoryManagementGuidance &guidance);

  TypedValue dynamicIsReusable(llvm::Value *what);
  void dynamicRetain(llvm::Value *objectPtr);
  llvm::Value *dynamicSuper(llvm::Value *objPtr);
  TypedValue dynamicRelease(llvm::Value *what, bool isAutorelease);
  llvm::Value *dynamicCond(llvm::Value *cond);

  TypedValue box(const TypedValue &value);
  TypedValue unbox(const TypedValue &value);
  std::pair<llvm::BasicBlock *, llvm::Value *> dynamicUnbox(const Node &node, const TypedValue &value, objectType forcedType);
  void runtimeException(const CodeGenerationException &runtimeException);  
  void logType(llvm::Value *v);
  void logDebugBoxed(llvm::Value *v);
  void logString(const std::string &s);
  TypedValue loadObjectFromRuntime(void *ptr);  
  static ObjectTypeSet typeOfObjectFromRuntime(void *ptr);

  uint64_t getClassId(const std::string &className);
  Class *getClass(uint64_t classId);

  llvm::Value *dynamicZero(const ObjectTypeSet &type);
  llvm::Value *callRuntimeFun(const std::string &fname, llvm::Type *retValType, const std::vector<llvm::Type *> &argTypes, const std::vector<llvm::Value *> &args, bool isVariadic = false);
  llvm::Value *callRuntimeFun(const std::string &fname, llvm::Type *retValType, const std::vector<std::pair<llvm::Type *, llvm::Value *>> &args);
  TypedValue callRuntimeFun(const std::string &fname, const ObjectTypeSet &retVal, const std::vector<TypedValue> &args);
  llvm::Value *dynamicCreate(objectType type, const std::vector<llvm::Type *> &argTypes, const std::vector<llvm::Value *> &args);
  llvm::Type *dynamicUnboxedType(objectType type);
  llvm::Type *dynamicBoxedType(objectType type);
  llvm::PointerType *dynamicBoxedType();
  llvm::Type *dynamicType(const ObjectTypeSet &type);

  /* Code generation */

  std::string codegenTopLevel(const Node &node, int i);

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
