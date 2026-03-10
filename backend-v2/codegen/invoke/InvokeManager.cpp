#include "InvokeManager.h"
#include "../../bridge/Exceptions.h"
#include "../../tools/EdnParser.h"
#include <vector>

using namespace llvm;
using namespace std;

namespace rt {

InvokeManager::InvokeManager(llvm::IRBuilder<> &b, llvm::Module &m,
                             ValueEncoder &v, LLVMTypes &t,
                             ThreadsafeCompilerState &s)
    : builder(b), theModule(m), valueEncoder(v), types(t) {
  intrinsics = {
      // Arithmetic
      {"FAdd",
       [](auto &b, auto args) { return b.CreateFAdd(args[0], args[1]); }},
      {"Add", [](auto &b, auto args) { return b.CreateAdd(args[0], args[1]); }},
      {"FSub",
       [](auto &b, auto args) { return b.CreateFSub(args[0], args[1]); }},
      {"Sub", [](auto &b, auto args) { return b.CreateSub(args[0], args[1]); }},
      {"FMul",
       [](auto &b, auto args) { return b.CreateFMul(args[0], args[1]); }},
      {"Mul", [](auto &b, auto args) { return b.CreateMul(args[0], args[1]); }},
      {"FDiv",
       [](auto &b, auto args) { return b.CreateFDiv(args[0], args[1]); }},
      {"Div",
       [](auto &b, auto args) { return b.CreateSDiv(args[0], args[1]); }},

      // Comparisons
      {"FCmpOGE",
       [](auto &b, auto args) { return b.CreateFCmpOGE(args[0], args[1]); }},
      {"ICmpSGE",
       [](auto &b, auto args) { return b.CreateICmpSGE(args[0], args[1]); }},
      {"FCmpOGT",
       [](auto &b, auto args) { return b.CreateFCmpOGT(args[0], args[1]); }},
      {"ICmpSGT",
       [](auto &b, auto args) { return b.CreateICmpSGT(args[0], args[1]); }},
      {"FCmpOLT",
       [](auto &b, auto args) { return b.CreateFCmpOLT(args[0], args[1]); }},
      {"FCmpOLE",
       [](auto &b, auto args) { return b.CreateFCmpOLE(args[0], args[1]); }},
      {"ICmpSLT",
       [](auto &b, auto args) { return b.CreateICmpSLT(args[0], args[1]); }},
      {"ICmpSLE",
       [](auto &b, auto args) { return b.CreateICmpSLE(args[0], args[1]); }},
      {"ICmpEQ",
       [](auto &b, auto args) { return b.CreateICmpEQ(args[0], args[1]); }},
      {"FCmpOEQ",
       [](auto &b, auto args) { return b.CreateFCmpOEQ(args[0], args[1]); }}};
}

TypedValue
InvokeManager::generateIntrinsic(const IntrinsicDescription &id,
                                 const std::vector<TypedValue> &args) {
  std::vector<ObjectTypeSet> argTypes;
  for (auto &arg : args)
    argTypes.push_back(arg.type.unboxed());

  if (id.type == CallType::Intrinsic) {
    auto block = intrinsics.find(id.symbol);
    if (block == intrinsics.end()) {
      throwInternalInconsistencyException("Intrinsic '" + id.symbol +
                                          "' does not exist.");
    }
    std::vector<llvm::Value *> argVals;
    for (auto &arg : args)
      argVals.push_back(valueEncoder.unbox(arg).value);
    return TypedValue(id.returnType, block->second(builder, argVals));
  } else if (id.type == CallType::Call) {
    return invokeRuntime(id.symbol, &id.returnType, argTypes, args);
  }

  throwInternalInconsistencyException("Unsupported call type");
}

TypedValue InvokeManager::invokeRuntime(
    const std::string &fname, const ObjectTypeSet *retValType,
    const std::vector<ObjectTypeSet> &argTypes,
    const std::vector<TypedValue> &args, const bool isVariadic) {
  if (argTypes.size() > args.size())
    throwInternalInconsistencyException(
        "Internal error: To litle arguments passed");
  std::vector<llvm::Type *> llvmTypes;
  for (auto &t : argTypes) {
    llvmTypes.push_back(types.typeForType(t));
  }

  if (!isVariadic && argTypes.size() != args.size())
    throwInternalInconsistencyException("Internal error: Wrong arg count");

  FunctionType *functionType = FunctionType::get(
      retValType ? types.typeForType(*retValType) : types.voidTy, llvmTypes,
      isVariadic);

  Function *toCall =
      theModule.getFunction(fname)
          ?: Function::Create(functionType, Function::ExternalLinkage, fname,
                              theModule);

  vector<llvm::Value *> argVals;
  for (size_t i = 0; i < args.size(); i++) {
    auto &arg = args[i];
    if (i < argTypes.size()) {
      auto &argType = argTypes[i];
      if (!argType.isBoxedType()) {
        if (!arg.type.isDetermined()) {
          std::ostringstream oss;
          oss << "Internal error: Types mismatch for runtime function call: "
                 "for "
              << i << "th arg required is " << argType.toString()
              << " and found " << arg.type.toString() << ".";
          throwInternalInconsistencyException(oss.str());
        }
        argVals.push_back(valueEncoder.unbox(arg).value);
      } else {
        /* Each boxed is allowed */
        argVals.push_back(valueEncoder.box(arg).value);
      }
    } else {
      /* Varargs are always boxed */
      argVals.push_back(valueEncoder.box(arg).value);
    }
  }
  return TypedValue(
      retValType ? *retValType : ObjectTypeSet::all(),
      builder.CreateCall(toCall, argVals, std::string("call_") + fname));
}
} // namespace rt
