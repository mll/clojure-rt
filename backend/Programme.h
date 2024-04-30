#ifndef RT_PROGRAMME
#define RT_PROGRAMME

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
#include "bytecode.pb.h"
#include <fstream>
#include <sstream>
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

extern "C" {
  #include "runtime/Class.h"
  typedef struct Var Var;
}

class CodeGenerator;
using namespace clojure::rt::protobuf::bytecode;

typedef std::pair<ObjectTypeSet, llvm::Value *> TypedValue;
typedef std::pair<ObjectTypeSet, const Node&> TypedNode;

typedef TypedValue (*StaticCall)(CodeGenerator *, const std::string &, const Node&, const std::vector<TypedValue> &);
typedef ObjectTypeSet (*StaticCallType)(CodeGenerator *, const std::string &, const Node&, const std::vector<ObjectTypeSet>&);

class ProgrammeState {
  public:
  /* TODO: thread safety? locks? */
  std::unordered_map<uint64_t, Node> Functions;
  std::unordered_map<uint64_t, Class *> DefinedClasses;
  std::unordered_map<std::string, uint64_t> ClassesByName;
  std::unordered_map<std::string, std::vector<ObjectTypeSet> > ClosedOverTypes;
  std::unordered_map<std::string, Op> RecurType;
  std::unordered_map<std::string, uint64_t> RecurTargets;
  std::unordered_map<std::string, uint64_t> StaticFunctions;
  std::unordered_map<std::string, ObjectTypeSet> FunctionsRetValInfers;
  std::unordered_map<std::string, std::pair<std::vector<ObjectTypeSet>, ObjectTypeSet>> LoopsBindingsAndRetValInfers;
  std::unordered_map<std::string, bool> RecursiveFunctionsNameMap;
  std::unordered_map<std::string, std::vector<std::pair<std::string, std::pair<StaticCallType, StaticCall>>>> StaticCallLibrary; // DEPRECATED?
  std::unordered_map<std::string, Var *> DefinedVarsByName;
  
  // TODO: Keep structure dynamic (updated as defrecord + others is used)
  std::unordered_map<
    uint64_t, // classId
    std::unordered_map<
      std::string, // methodName
      std::vector<
        std::pair<
          std::string, // signature
          std::pair<StaticCallType, StaticCall>>>>> InstanceCallLibrary;
          
  std::unordered_map<
    uint64_t, // classId
    std::unordered_map<
      std::string, // methodName
      std::vector<
        std::pair<
          std::string, // signature
          void *>>>> DynamicCallLibrary;

  uint64_t lastFunctionUniqueId = 0;
  uint64_t lastClassUniqueId = 100; // reserved for ANY and primitive types

  ProgrammeState();
  // ~ProgrammeState();
  
  static std::string closedOverKey(uint64_t functionId, uint64_t methodId);
  uint64_t getUniqueClassId();
  uint64_t registerClass(Class *_class);
  
  uint64_t getClassId(const std::string &className);
  Class *getClass(uint64_t classId);
  
  void *getPrimitiveMethod(objectType target, const std::string &methodName, const std::vector<objectType> &argTypes);
  Var *getVarByName(const std::string &varName);
  std::pair<Var *, BOOL> getVar(const std::string &varName); // second is TRUE if new (unbound) var was created
};

#endif
