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
  std::unordered_map<std::pair<uint64_t, uint64_t>, std::vector<ObjectTypeSet> > ClosedOverTypes;
  std::unordered_map<std::string, uint64_t> RecurTargets;
  std::unordered_map<std::string, uint64_t> StaticFunctions;
  std::unordered_map<std::string, ObjectTypeSet> RecursiveFunctionsRetValGuesses;
  std::unordered_map<std::string, bool> RecursiveFunctionsNameMap;
  std::unordered_map<std::string, std::vector<std::pair<std::string, std::pair<StaticCallType, StaticCall>>>> StaticCallLibrary; 
  std::unordered_map<std::string, ObjectTypeSet> StaticVarTypes;

  uint64_t lastFunctionUniqueId = 0;

  ProgrammeState();
};

#endif
