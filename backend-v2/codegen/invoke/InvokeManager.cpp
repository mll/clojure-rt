#include "InvokeManager.h"
#include "../../bridge/Exceptions.h"
#include <vector>

using namespace llvm;
using namespace std;

namespace rt {

InvokeManager::InvokeManager(llvm::IRBuilder<> &b, llvm::Module &m,
                             ValueEncoder &v, LLVMTypes &t,
                             ThreadsafeCompilerState &s)
    : builder(b), theModule(m), valueEncoder(v), types(t), state(s) {
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
      {"FCmpOLT",
       [](auto &b, auto args) { return b.CreateFCmpOLT(args[0], args[1]); }},
      {"ICmpSLT",
       [](auto &b, auto args) { return b.CreateICmpSLT(args[0], args[1]); }}};
}

ObjectTypeSet InvokeManager::returnValueType(PersistentArrayMap *description) {
  RTValue returnValueTypeIndicator = PersistentArrayMap_get(
      description, Keyword_create(String_create("returns")));

  objectType returnValueTypeIndicatorType = getType(returnValueTypeIndicator);
  if (returnValueTypeIndicatorType != keywordType &&
      returnValueTypeIndicatorType != symbolType) {
    release(returnValueTypeIndicator);
    throwInternalInconsistencyException(
        "Return type must be an alias or a symbol");
  }

  String *compactifiedReturnTypeIndicatorString =
      String_compactify(toString(returnValueTypeIndicator));

  PersistentArrayMap *returnValueType = state.internalClassRegistry.getCurrent(
      String_c_str(compactifiedReturnTypeIndicatorString));
  Ptr_release(compactifiedReturnTypeIndicatorString);

  RTValue returnValueTypeEnum = PersistentArrayMap_get(
      returnValueType, Keyword_create(String_create("object-type")));

  if (getType(returnValueTypeEnum) != integerType) {
    release(returnValueTypeEnum);
    throwInternalInconsistencyException(
        "Return value type must be an internal type");
  }

  ObjectTypeSet returnValueTypeSet(
      (objectType)RT_unboxInt32(returnValueTypeEnum));
  /* Only a formality, integers are not memory managed */
  release(returnValueTypeEnum);
  return returnValueTypeSet;
}

bool InvokeManager::checkIntrinsicArgs(PersistentArrayMap *description,
                                       const std::vector<ObjectTypeSet> &args) {
  ThreadsafeRegistry<PersistentArrayMap> &internalClassRegistry =
      state.internalClassRegistry;

  RTValue argsVecRaw = PersistentArrayMap_get(
      description, Keyword_create(String_create("args")));

  if (getType(argsVecRaw) != persistentVectorType) {
    release(argsVecRaw);
    throwInternalInconsistencyException(
        "Intrinsic call: :args is not a vector");
  }

  PersistentVector *argsVec = (PersistentVector *)RT_unboxPtr(argsVecRaw);

  if (PersistentVector_count(argsVec) != args.size()) {
    release(argsVecRaw);
    return false;
  }

  PersistentVectorIterator it = PersistentVector_iterator(argsVec);

  for (size_t i = 0; i < args.size(); i++) {
    RTValue argRaw = PersistentVector_iteratorGet(&it);
    auto &arg = args[i];
    objectType argType = getType(argRaw);
    if (argType != keywordType && argType != symbolType) {
      release(argsVecRaw);
      return false;
    }

    if (!arg.isDetermined()) {
      RTValue anyKeyword = Keyword_create(String_create("any"));
      if (equals(argRaw, anyKeyword)) {
        release(anyKeyword);
        PersistentVector_iteratorNext(&it);
        continue;
      }
      release(anyKeyword);
      release(argsVecRaw);
      return false;
    }

    PersistentArrayMap *argClass =
        internalClassRegistry.getCurrent((int32_t)arg.determinedType());
    /* Intrinsics are supported only for built-in objects (represented by
     * ObjectTypeSet) */
    if (!argClass) {
      release(argsVecRaw);
      throwInternalInconsistencyException(
          "Intrinsic call: only basic types can be used in the intrinsics");
    }
    Ptr_retain(argClass);
    RTValue className =
        PersistentArrayMap_get(argClass, Keyword_create(String_create("name")));
    RTValue classAlias = PersistentArrayMap_get(
        argClass, Keyword_create(String_create("alias")));

    if (!equals(className, argRaw) && !equals(classAlias, argRaw)) {
      release(classAlias);
      release(className);
      release(argsVecRaw);
      return false;
    }
    release(classAlias);
    release(className);
    PersistentVector_iteratorNext(&it);
  }
  release(argsVecRaw);
  return true;
}

TypedValue
InvokeManager::generateIntrinsic(RTValue intrinsicDescription,
                                 const std::vector<TypedValue> &args) {
  std::vector<ObjectTypeSet> argTypes;
  for (auto arg : args) {
    argTypes.push_back(arg.type.unboxed());
  }

  if (getType(intrinsicDescription) != persistentArrayMapType) {
    release(intrinsicDescription);
    throwInternalInconsistencyException(
        "Intrinsic call: description is not a map");
  }
  PersistentArrayMap *description =
      (PersistentArrayMap *)RT_unboxPtr(intrinsicDescription);
  Ptr_retain(description);

  try {
    /* TODO: Ultimately we will assume that arg count and types match at the
     * moment of generation. For now check stays here for debugging. */
    Ptr_retain(description);
    if (!checkIntrinsicArgs(description, argTypes)) {
      throwInternalInconsistencyException("Args do not match");
    }

    Ptr_retain(description);
    ObjectTypeSet returnValueTypeSet = returnValueType(description);

    RTValue intrinsicName = PersistentArrayMap_get(
        description, Keyword_create(String_create("symbol")));
    if (getType(intrinsicName) != stringType) {
      release(intrinsicName);
      throwInternalInconsistencyException("Symbol is not a string");
    }
    String *compactifiedIntrinsicName =
        String_compactify((String *)RT_unboxPtr(intrinsicName));
    std::string stringIntrinsicName(String_c_str(compactifiedIntrinsicName));
    Ptr_release(compactifiedIntrinsicName);

    Ptr_retain(description);
    RTValue callType = PersistentArrayMap_get(
        description, Keyword_create(String_create("type")));
    RTValue intriniscKeyword = Keyword_create(String_create("intrinsic"));
    RTValue callKeyword = Keyword_create(String_create("call"));

    if (equals(callType, intriniscKeyword)) {
      auto block = intrinsics.find(stringIntrinsicName);
      if (block == intrinsics.end()) {
        release(intriniscKeyword);
        release(callKeyword);
        throwInternalInconsistencyException(
            "Intrinsic '" + stringIntrinsicName + "' does not exist.");
      }
      std::vector<llvm::Value *> argVals;
      for (auto arg : args) {
        argVals.push_back(valueEncoder.unbox(arg).value);
      }

      release(intriniscKeyword);
      release(callKeyword);
      Ptr_release(description);
      return TypedValue(returnValueTypeSet, block->second(builder, argVals));

    } else if (equals(callType, callKeyword)) {
      release(intriniscKeyword);
      release(callKeyword);
      Ptr_release(description);
      return invokeRuntime(stringIntrinsicName, &returnValueTypeSet, argTypes,
                           args);
    } else {
      release(intriniscKeyword);
      release(callType);
      throwInternalInconsistencyException("Unknown intrinsic type.");
    }
  } catch (...) {
    Ptr_release(description);
    throw;
  }
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
