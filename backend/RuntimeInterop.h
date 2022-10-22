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
#include "runtime/defines.h"

using namespace std;
using namespace llvm;
using namespace llvm::orc;
using namespace clojure::rt::protobuf::bytecode;

class RuntimeInterop {
  public:
  RuntimeInterop();
  vector<TypedValue> codegen(const Programme &programme);
  TypedValue codegen(const Node &node);

  TypedValue codegen(const Node &node, const BindingNode &subnode);
  TypedValue codegen(const Node &node, const CaseNode &subnode);
  TypedValue codegen(const Node &node, const CaseTestNode &subnode);
  TypedValue codegen(const Node &node, const CaseThenNode &subnode);
  TypedValue codegen(const Node &node, const CatchNode &subnode);
  TypedValue codegen(const Node &node, const ConstNode &subnode);
  TypedValue codegen(const Node &node, const DefNode &subnode);
  TypedValue codegen(const Node &node, const DeftypeNode &subnode);
  TypedValue codegen(const Node &node, const DoNode &subnode);
  TypedValue codegen(const Node &node, const FnNode &subnode);
  TypedValue codegen(const Node &node, const FnMethodNode &subnode);
  TypedValue codegen(const Node &node, const HostInteropNode &subnode);
  TypedValue codegen(const Node &node, const IfNode &subnode);
  TypedValue codegen(const Node &node, const ImportNode &subnode);
  TypedValue codegen(const Node &node, const InstanceCallNode &subnode);
  TypedValue codegen(const Node &node, const InstanceFieldNode &subnode);
  TypedValue codegen(const Node &node, const IsInstanceNode &subnode);
  TypedValue codegen(const Node &node, const InvokeNode &subnode);
  TypedValue codegen(const Node &node, const KeywordInvokeNode &subnode);
  TypedValue codegen(const Node &node, const LetNode &subnode);
  TypedValue codegen(const Node &node, const LetfnNode &subnode);
  TypedValue codegen(const Node &node, const LocalNode &subnode);
  TypedValue codegen(const Node &node, const LoopNode &subnode);
  TypedValue codegen(const Node &node, const MapNode &subnode);
  TypedValue codegen(const Node &node, const MethodNode &subnode);
  TypedValue codegen(const Node &node, const MonitorEnterNode &subnode);
  TypedValue codegen(const Node &node, const MonitorExitNode &subnode);
  TypedValue codegen(const Node &node, const NewNode &subnode);
  TypedValue codegen(const Node &node, const PrimInvokeNode &subnode);
  TypedValue codegen(const Node &node, const ProtocolInvokeNode &subnode);
  TypedValue codegen(const Node &node, const QuoteNode &subnode);
  TypedValue codegen(const Node &node, const RecurNode &subnode);
  TypedValue codegen(const Node &node, const ReifyNode &subnode);
  TypedValue codegen(const Node &node, const SetNode &subnode);
  TypedValue codegen(const Node &node, const MutateSetNode &subnode);
  TypedValue codegen(const Node &node, const StaticCallNode &subnode);
  TypedValue codegen(const Node &node, const StaticFieldNode &subnode);
  TypedValue codegen(const Node &node, const TheVarNode &subnode);
  TypedValue codegen(const Node &node, const ThrowNode &subnode);
  TypedValue codegen(const Node &node, const TryNode &subnode);
  TypedValue codegen(const Node &node, const VarNode &subnode);
  TypedValue codegen(const Node &node, const VectorNode &subnode);
  TypedValue codegen(const Node &node, const WithMetaNode &subnode);
};

#endif
