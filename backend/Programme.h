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
#include "protobuf/bytecode.pb.h"
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
using namespace llvm;

typedef pair<ObjectTypeSet, Value *> TypedValue;
typedef pair<ObjectTypeSet, const Node&> TypedNode;

typedef TypedValue (*StaticCall)(CodeGenerator *, const string &, const Node&, const std::vector<TypedNode>&);
typedef ObjectTypeSet (*StaticCallType)(CodeGenerator *, const string &, const Node&, const std::vector<TypedNode>&);

class ProgrammeState {
  public:
  /* TODO: thread safety? locks? */
  std::unordered_map<std::string, Node> Functions;
  std::unordered_map<std::string, std::string> StaticFunctions;
  std::unordered_map<std::string, std::string> NodesToFunctions;
  std::unordered_map<string, ObjectTypeSet> RecursiveFunctionsRetValGuesses;
  std::unordered_map<string, bool> RecursiveFunctionsNameMap;
  unordered_map<string, vector<pair<string, pair<StaticCallType, StaticCall>>>> StaticCallLibrary; 
  std::unordered_map<std::string, ObjectTypeSet> StaticVarTypes;

  uint64_t lastFunctionUniqueId = 0;

  ProgrammeState();
};

#endif
