#include "InvokeManager.h"
#include "../../bridge/Exceptions.h"

using namespace llvm;
using namespace std;

namespace rt {

  InvokeManager::InvokeManager(llvm::IRBuilder<> &b, llvm::Module &m,
                               ValueEncoder &v, LLVMTypes &t)
    : builder(b), theModule(m), valueEncoder(v), types(t) {}


  TypedValue
  InvokeManager::invokeRuntime(const std::string &fname,
                               const ObjectTypeSet *retValType,
                               const std::vector<ObjectTypeSet> &argTypes,
                               const std::vector<TypedValue> &args,
                               const bool isVariadic) {
    if(argTypes.size() > args.size()) throwInternalInconsistencyException("Internal error: To litle arguments passed");
    std::vector<llvm::Type *> llvmTypes;
    for (auto &t : argTypes) {
      llvmTypes.push_back(types.typeForType(t));
    }

    if(!isVariadic && argTypes.size() != args.size()) throwInternalInconsistencyException("Internal error: Wrong arg count");
    
    FunctionType *functionType = FunctionType::get(
        retValType ? types.typeForType(*retValType) : types.voidTy, llvmTypes, isVariadic);

    Function *toCall =
      theModule.getFunction(fname)
      ?: Function::Create(functionType, Function::ExternalLinkage,
                          fname, theModule);

    vector<llvm::Value *> argVals;
    for (size_t i = 0; i < args.size(); i++) {
      auto &arg = args[i];
      if (i < argTypes.size()) {
        auto &argType = argTypes[i];
        if (!argType.isBoxedType()) {
           if(!arg.type.isDetermined())
             throwInternalInconsistencyException(std::format(
                 "Internal error: Types mismatch for runtime function "
                 "call: for {}th arg required is {} and found {}.",
                 i, argType.toString(), arg.type.toString()));
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
}
