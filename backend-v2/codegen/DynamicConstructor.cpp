#include "DynamicConstructor.h"
#include "../RuntimeHeaders.h"
#include "../bridge/Exceptions.h"
#include "../cljassert.h"
#include "../runtime/word.h"
#include "LLVMTypes.h"
#include "TypedValue.h"
#include "ValueEncoder.h"
#include "invoke/InvokeManager.h"
#include "llvm/ADT/StringSwitch.h"
#include <cstdint>
#include <llvm/IR/Constants.h>
#include <string>

using namespace llvm;

namespace rt {

DynamicConstructor::DynamicConstructor(LLVMTypes &t, InvokeManager &i,
                                       std::vector<RTValue> &gc)
    : types(t), invokeManager(i), generatedConstants(gc) {}

TypedValue DynamicConstructor::createDouble(const char *s) {
  return TypedValue(
      ObjectTypeSet(doubleType, false,
                    new ConstantDouble(std::stod(std::string(s)))),
      ConstantFP::get(types.doubleTy, StringRef(s)));
}

TypedValue DynamicConstructor::createDouble(const double d) {
  return TypedValue(ObjectTypeSet(doubleType, false, new ConstantDouble(d)),
                    ConstantFP::get(types.doubleTy, d));
}

TypedValue DynamicConstructor::createInt32(const char *s) {
  return TypedValue(
      ObjectTypeSet(integerType, false,
                    new ConstantInteger(std::stoi(std::string(s)))),
      ConstantInt::get(types.i32Ty, APInt(32, StringRef(s), 10)));
}

TypedValue DynamicConstructor::createInt32(const int32_t i) {
  return TypedValue(ObjectTypeSet(integerType, false, new ConstantInteger(i)),
                    ConstantInt::get(types.i32Ty, i));
}

TypedValue DynamicConstructor::createNil() {
  return TypedValue(ObjectTypeSet(nilType, true, new ConstantNil()),
                    ConstantInt::get(types.i64Ty, RT_TAG_NIL));
}

TypedValue DynamicConstructor::createBoolean(const char *s) {
  bool retVal = StringSwitch<bool>(s).Case("true", true).Default(false);
  return TypedValue(
      ObjectTypeSet(booleanType, false, new ConstantBoolean(retVal)),
      ConstantInt::get(types.i1Ty, retVal));
}

TypedValue DynamicConstructor::createBoolean(const bool b) {
  return TypedValue(ObjectTypeSet(booleanType, false, new ConstantBoolean(b)),
                    ConstantInt::get(types.i1Ty, b));
}

TypedValue DynamicConstructor::createString(const char *s) {
  String *str = String_createDynamicStr(s);
  generatedConstants.push_back(RT_boxPtr(str));
  uintptr_t address = reinterpret_cast<uintptr_t>(str);
  return TypedValue(
      ObjectTypeSet(stringType, false, new ConstantString(std::string(s))),
      ConstantExpr::getIntToPtr(ConstantInt::get(types.i64Ty, address),
                                types.ptrTy));
}

TypedValue DynamicConstructor::createKeyword(const char *s) {
  String *str = String_createDynamicStr(s);
  RTValue kwVal = Keyword_create(str);
  uint32_t retVal = RT_unboxKeyword(kwVal);
  return TypedValue(ObjectTypeSet(keywordType, false, new ConstantKeyword(s)),
                    ConstantInt::get(types.i32Ty, retVal));
}
TypedValue DynamicConstructor::createSymbol(const char *s) {
  String *str = String_createDynamicStr(s);
  RTValue symVal = Symbol_create(str);
  uint32_t retVal = RT_unboxSymbol(symVal);
  return TypedValue(ObjectTypeSet(symbolType, false, new ConstantSymbol(s)),
                    ConstantInt::get(types.i32Ty, retVal));
}

TypedValue DynamicConstructor::createBigInteger(const char *s) {
  BigInteger *retVal = BigInteger_createFromStr(s);
  generatedConstants.push_back(RT_boxPtr(retVal));
  uintptr_t address = reinterpret_cast<uintptr_t>(retVal);
  return TypedValue(ObjectTypeSet(bigIntegerType, false,
                                  new ConstantBigInteger(std::string(s))),
                    ConstantExpr::getIntToPtr(
                        ConstantInt::get(types.i64Ty, address), types.ptrTy));
}

TypedValue DynamicConstructor::createRatio(const char *s) {
  Ratio *retVal = Ratio_createFromStr(s);
  generatedConstants.push_back(RT_boxPtr(retVal));
  uintptr_t address = reinterpret_cast<uintptr_t>(retVal);
  return TypedValue(
      ObjectTypeSet(ratioType, false, new ConstantRatio(std::string(s))),
      ConstantExpr::getIntToPtr(ConstantInt::get(types.i64Ty, address),
                                types.ptrTy));
}

TypedValue DynamicConstructor::createVector(std::vector<TypedValue> &items) {
  auto retValType = ObjectTypeSet(persistentVectorType, false);
  std::vector<TypedValue> allArgs;
  allArgs.push_back(createInt32(items.size()));
  for (auto &item : items)
    allArgs.push_back(item);
  return invokeManager.invokeRuntime("PersistentVector_createMany", &retValType,
                                     {ObjectTypeSet(integerType, false)},
                                     allArgs, true);
}

TypedValue DynamicConstructor::createArrayMap(std::vector<TypedValue> &keys,
                                              std::vector<TypedValue> &values) {
  if (keys.size() != values.size())
    throwInternalInconsistencyException(
        "Keys and values need to have the same size");

  if (keys.size() > HASHTABLE_THRESHOLD)
    throwInternalInconsistencyException(
        "PersistentArrayMap can store at most " +
        std::to_string(HASHTABLE_THRESHOLD) + " key-value pairs");
  auto retValType = ObjectTypeSet(persistentArrayMapType, false);
  std::vector<TypedValue> allArgs;
  allArgs.push_back(createInt32(keys.size()));

  for (size_t i = 0; i < keys.size(); i++) {
    allArgs.push_back(keys[i]);
    allArgs.push_back(values[i]);
  }

  return invokeManager.invokeRuntime(
      "PersistentArrayMap_createMany", &retValType,
      {ObjectTypeSet(integerType, false)}, allArgs, true);
}

TypedValue DynamicConstructor::createList(std::vector<TypedValue> &items) {
  auto retValType = ObjectTypeSet(persistentListType, false);
  std::vector<TypedValue> allArgs;
  allArgs.push_back(createInt32(items.size()));
  for (auto &item : items)
    allArgs.push_back(item);
  return invokeManager.invokeRuntime("PersistentList_createMany", &retValType,
                                     {ObjectTypeSet(integerType, false)},
                                     allArgs, true);
}

} // namespace rt
