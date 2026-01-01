
#include "DynamicConstructor.h"
#include <llvm/IR/Constants.h>
#include <string>
#include <sys/_types/_int64_t.h>
#include "../runtime/word.h"
#include "../cljassert.h"
#include "LLVMTypes.h"
#include "TypedValue.h"
#include "ValueEncoder.h"
#include "llvm/ADT/StringSwitch.h"
#include "../RuntimeHeaders.h"
#include "../bridge/Exceptions.h"

using namespace llvm;

namespace rt {

  DynamicConstructor::DynamicConstructor(IRBuilder<>& b, Module &m, ValueEncoder &v, LLVMTypes &t) 
    : builder(b), theModule(m), valueEncoder(v), types(t) {}

  TypedValue DynamicConstructor::createDouble(const char *s) {
    return TypedValue(
        ObjectTypeSet(doubleType, false,
                      new ConstantDouble(std::stod(std::string(s)))),
        ConstantFP::get(types.doubleTy, StringRef(s)));    
  }

  TypedValue DynamicConstructor::createDouble(const double d) {
    return TypedValue(
                      ObjectTypeSet(doubleType, false,
                      new ConstantDouble(d)),
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
                      ConstantInt::get(types.i64Ty, ValueEncoder::TAG_NIL));
    
  }

  TypedValue DynamicConstructor::createBoolean(const char *s) {
    bool retVal = StringSwitch<bool>(s).Case("true", true).Default(false);
    return TypedValue(
        ObjectTypeSet(booleanType, false, new ConstantBoolean(retVal)),
        ConstantInt::get(types.i1Ty, retVal));
  }
  
  TypedValue DynamicConstructor::createBoolean(const bool b) {
    return TypedValue(
        ObjectTypeSet(booleanType, false, new ConstantBoolean(b)),
        ConstantInt::get(types.i1Ty, b));
  }
  
  TypedValue DynamicConstructor::createString(const char *s) {
    String *str = String_createDynamicStr(s);
    uintptr_t address = reinterpret_cast<uintptr_t>(str);
    return TypedValue(
        ObjectTypeSet(stringType, false, new ConstantString(std::string(s))),
        ConstantExpr::getIntToPtr(ConstantInt::get(types.i64Ty, address), types.ptrTy));
  }

  TypedValue DynamicConstructor::createKeyword(const char *s) {
    String *str = String_createDynamicStr(s);
    uint32_t retVal = RT_unboxKeyword(Keyword_create(str));
    return TypedValue(
        ObjectTypeSet(keywordType, false, new ConstantKeyword(s)),
        ConstantInt::get(types.i32Ty, retVal));    
  }
  TypedValue DynamicConstructor::createSymbol(const char *s) {
    String *str = String_createDynamicStr(s);
    uint32_t retVal = RT_unboxSymbol(Symbol_create(str));
    return TypedValue(
                      ObjectTypeSet(symbolType, false, new ConstantSymbol(s)),
                      ConstantInt::get(types.i32Ty, retVal));
  }

  TypedValue DynamicConstructor::createBigInteger(const char *s) {
    BigInteger *retVal = BigInteger_createFromStr(s);
    uintptr_t address = reinterpret_cast<uintptr_t>(retVal);
    return TypedValue(
        ObjectTypeSet(bigIntegerType, false,
                      new ConstantBigInteger(std::string(s))),
        ConstantExpr::getIntToPtr(ConstantInt::get(types.i64Ty, address), types.ptrTy));
  }

  TypedValue DynamicConstructor::createRatio(const char *s) {
    Ratio *retVal = Ratio_createFromStr(s);
    uintptr_t address = reinterpret_cast<uintptr_t>(retVal);
    return TypedValue(
        ObjectTypeSet(bigIntegerType, false,
                      new ConstantRatio(std::string(s))),
        ConstantExpr::getIntToPtr(ConstantInt::get(types.i64Ty, address), types.ptrTy));
  }

  TypedValue DynamicConstructor::createVector(std::vector<TypedValue> &items) {
    std::string fname = "PersistentVector_createMany";
    FunctionType *functionType = FunctionType::get(types.ptrTy, {types.wordTy}, true);
    Function *toCall =
        theModule.getFunction(fname)
            ?: Function::Create(functionType, Function::ExternalLinkage,
                                fname, theModule);

    std::vector<Value *> args;
    args.push_back(ConstantInt::get(types.wordTy, items.size()));
    for (auto &item : items) {      
      args.push_back(valueEncoder.box(item).value);
    }
    return TypedValue(ObjectTypeSet(persistentVectorType, false),
                      builder.CreateCall(toCall, args, std::string("call_") + fname));
  }
  
  
  TypedValue
  DynamicConstructor::createArrayMap(std::vector<TypedValue> &keys,
                                     std::vector<TypedValue> &values) {
    if (keys.size() != values.size())
      throwInternalInconsistencyException(
                                          "Keys and values need to have the same size");
    
    if (keys.size() > HASHTABLE_THRESHOLD) throwInternalInconsistencyException(
                                                                               "PersistentArrayMap can store at most " + std::to_string(HASHTABLE_THRESHOLD) + " key-value pairs");
    std::string fname = "PersistentArrayMap_createMany";
    FunctionType *functionType = FunctionType::get(types.ptrTy, {types.wordTy}, true);
    Function *toCall =
      theModule.getFunction(fname)
      ?: Function::Create(functionType, Function::ExternalLinkage,
                          fname, theModule);
    
    std::vector<Value *> args;
    args.push_back(ConstantInt::get(types.wordTy, keys.size()));
    for (size_t i = 0; i < keys.size(); i++) {
      args.push_back(valueEncoder.box(keys[i]).value);
      args.push_back(valueEncoder.box(values[i]).value);      
    }
    return TypedValue(ObjectTypeSet(persistentArrayMapType, false),
                      builder.CreateCall(toCall, args, std::string("call_") + fname));
  }
  TypedValue DynamicConstructor::createList(std::vector<TypedValue> &items) {
    std::string fname = "PersistentList_createMany";
    FunctionType *functionType = FunctionType::get(types.ptrTy, {types.wordTy}, true);
    Function *toCall =
        theModule.getFunction(fname)
            ?: Function::Create(functionType, Function::ExternalLinkage,
                                fname, theModule);

    std::vector<Value *> args;
    args.push_back(ConstantInt::get(types.wordTy, items.size()));
    for (auto &item : items) {      
      args.push_back(valueEncoder.box(item).value);
    }
    return TypedValue(ObjectTypeSet(persistentVectorType, false),
                      builder.CreateCall(toCall, args, std::string("call_") + fname));
  }    
} // namespace rt


