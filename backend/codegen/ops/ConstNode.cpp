#include "../../bridge/Exceptions.h"
#include "../../tools/RTValueWrapper.h"
#include "../CodeGen.h"
#include "codegen/TypedValue.h"
#include "state/ThreadsafeCompilerState.h"
#include "types/ConstantClass.h"
#include "types/ObjectTypeSet.h"
#include <string>

using namespace std;
using namespace llvm;
using namespace clojure::rt::protobuf::bytecode;

namespace rt {
TypedValue CodeGen::codegen(const Node &node, const ConstNode &subnode,
                            const ObjectTypeSet &typeRestrictions) {
  auto types = getType(node, subnode, typeRestrictions);

  if (types.size() == 0) {
    throwCodeGenerationException(
        string("This type of constant cannot be inserted because of type "
               "mismatch:  ") +
            typeRestrictions.toString(),
        node);
  }

  if (!types.isDetermined()) {
    throwCodeGenerationException(
        string("Constant type must be well defined. Instead we have found a "
               "composite type: ") +
            types.toString(),
        node);
  }

  auto name = subnode.val();
  TypedValue retVal(ObjectTypeSet::all(), nullptr);

  /* Heap objects need to have an extra retain generated into the code, because
     a new retain is needed every time the node is *executed* not compiled */
  switch (types.determinedType()) {
  case integerType:
    retVal = dynamicConstructor.createInt32(subnode.val().c_str());
    break;
  case doubleType:
    retVal = dynamicConstructor.createDouble(subnode.val().c_str());
    break;
  case nilType:
    retVal = dynamicConstructor.createNil();
    break;
  case booleanType:
    retVal = dynamicConstructor.createBoolean(subnode.val().c_str());
    break;
  case stringType:
    retVal = dynamicConstructor.createString(subnode.val().c_str());
    memoryManagement.dynamicRetain(retVal);
    break;
  case symbolType:
    retVal = dynamicConstructor.createSymbol(name.c_str());
    memoryManagement.dynamicRetain(retVal);
    break;
  case classType: {
    string className = name;
    if (className.find("class ") == 0) {
      className = className.substr(6);
    }
    ::Class *cls = compilerState.classRegistry.getCurrent(className.c_str());
    if (!cls) {
      throwCodeGenerationException(string("Unable to resolve class: ") +
                                       className + " in this context",
                                   node);
    }
    retVal = dynamicConstructor.createClass(cls, className);
    memoryManagement.dynamicRetain(retVal);
  } break;
  // case deftypeType:
  //   throwCodeGenerationException(
  //                                string("Not possible to create const of
  //                                type: ") +
  //                                ConstNode_ConstType_Name(subnode.type()),
  //                                node);
  //   break;
  case keywordType:
    retVal = dynamicConstructor.createKeyword(
        (name[0] == ':' ? name.substr(1) : name).c_str());
    break;
  case bigIntegerType:
    retVal = dynamicConstructor.createBigInteger(subnode.val().c_str());
    memoryManagement.dynamicRetain(retVal);
    break;
  case ratioType:
    retVal = dynamicConstructor.createRatio(subnode.val().c_str());
    memoryManagement.dynamicRetain(retVal);
    break;
  case varType: {
    const std::string name = subnode.val();
    ScopedRef<Var> var(compilerState.getCurrentVar(name.c_str()));
    if (!var) {
      throwCodeGenerationException(
          string("Unable to resolve var: ") + name + " in this context", node);
    }
    uint64_t address = reinterpret_cast<uint64_t>(var.get());
    retVal = TypedValue(
        ObjectTypeSet(varType),
        ConstantExpr::getIntToPtr(ConstantInt::get(this->types.i64Ty, address),
                                  this->types.ptrTy));
    memoryManagement.dynamicRetain(retVal);
    break;
  }
  case persistentVectorType:
    if (subnode.val() == "[]") {
      std::vector<TypedValue> items;
      retVal = dynamicConstructor.createVector(items);
    } else {
      throwCodeGenerationException(
          string("Compiler does not support non-empty vector constants yet: ") +
              subnode.val(),
          node);
    }
    break;
  case persistentArrayMapType:
    if (subnode.val() == "{}") {
      std::vector<TypedValue> empty;
      retVal = dynamicConstructor.createArrayMap(empty, empty);
    } else {
      throwCodeGenerationException(
          string("Compiler does not support non-empty map constants yet: ") +
              subnode.val(),
          node);
    }
    break;
  default:
    throwCodeGenerationException(
        string("Compiler does not support the following const type yet: ") +
            to_string(types.determinedType()),
        node);
  }
  if (subnode.has_meta()) {
    CleanupChainGuard guard(*this);
    guard.push(retVal);
    TypedValue meta = codegen(subnode.meta(), ObjectTypeSet::all());
    guard.push(meta);

    string fname;
    ObjectTypeSet retValType = retVal.type;

    switch (types.determinedType()) {
    case symbolType:
      fname = "Symbol_withMeta";
      break;
    case persistentVectorType:
      fname = "PersistentVector_withMeta";
      break;
    case persistentListType:
      fname = "PersistentList_withMeta";
      break;
    case persistentArrayMapType:
      fname = "PersistentArrayMap_withMeta";
      break;
    case varType:
      fname = "Var_resetMeta";
      break;
    default:
      // Type does not support metadata. Release meta and return original value.
      memoryManagement.dynamicRelease(meta);
      return retVal;
    }

    retVal = invokeManager.invokeRuntime(
        fname, &retValType, {retVal.type, ObjectTypeSet::dynamicType()},
        {retVal, meta}, false, &guard);
  }

  return retVal;
}

ObjectTypeSet CodeGen::getType(const Node &node, const ConstNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {

  switch (subnode.type()) {
  case ConstNode_ConstType_constTypeNumber:
    if (subnode.val().find('/') != string::npos) {
      return ObjectTypeSet(ratioType, true, new ConstantRatio(subnode.val()))
          .restriction(typeRestrictions);
    }

    if (node.tag() == "clojure.lang.Ratio" ||
        node.otag() == "clojure.lang.Ratio" ||
        node.tag() == "class clojure.lang.Ratio" ||
        node.otag() == "class clojure.lang.Ratio") {
      return ObjectTypeSet(ratioType, false, new ConstantRatio(subnode.val()))
          .restriction(typeRestrictions);
    }

    if (node.tag() == "clojure.lang.BigInt" ||
        node.otag() == "clojure.lang.BigInt" ||
        node.tag() == "class clojure.lang.BigInt" ||
        node.otag() == "class clojure.lang.BigInt") {
      return ObjectTypeSet(bigIntegerType, false,
                           new ConstantBigInteger(subnode.val()))
          .restriction(typeRestrictions);
    }

    if (node.tag() == "double" || node.otag() == "double" ||
        node.tag() == "class java.lang.Double" ||
        subnode.val().find('.') != string::npos) {
      return ObjectTypeSet(doubleType, false,
                           new ConstantDouble(stod(subnode.val())))
          .restriction(typeRestrictions);
    }

    if (node.tag() == "long" || node.otag() == "long" ||
        node.tag() == "class java.lang.Long" || node.tag() == "" ||
        node.tag() == "class java.lang.Integer") {
      try {
        size_t end;
        long long val = stoll(subnode.val(), &end);
        if (end == subnode.val().size()) {
          if (val >= INT32_MIN && val <= INT32_MAX) {
            return ObjectTypeSet(integerType, false,
                                 new ConstantInteger((int32_t)val))
                .restriction(typeRestrictions);
          } else {
            return ObjectTypeSet(bigIntegerType, false,
                                 new ConstantBigInteger(subnode.val()))
                .restriction(typeRestrictions);
          }
        }
      } catch (...) {
        if (!subnode.val().empty() &&
            (isdigit(subnode.val()[0]) || subnode.val()[0] == '-')) {
          return ObjectTypeSet(bigIntegerType, true,
                               new ConstantBigInteger(subnode.val()))
              .restriction(typeRestrictions);
        }
      }
    }

    throwCodeGenerationException(
        string("Compiler only supports 32 bit integers at this time. "), node);
    break;

  case ConstNode_ConstType_constTypeNil:
    return ObjectTypeSet(nilType, false, new ConstantNil())
        .restriction(typeRestrictions);
  case ConstNode_ConstType_constTypeBool:
    return ObjectTypeSet(booleanType, false,
                         new ConstantBoolean(subnode.val() == "true"))
        .restriction(typeRestrictions);
  case ConstNode_ConstType_constTypeString:
    return ObjectTypeSet(stringType, false, new ConstantString(subnode.val()))
        .restriction(typeRestrictions);
  case ConstNode_ConstType_constTypeSymbol:
    return ObjectTypeSet(symbolType, false, new ConstantSymbol(subnode.val()))
        .restriction(typeRestrictions);
  case ConstNode_ConstType_constTypeKeyword:
    return ObjectTypeSet(keywordType, false, new ConstantKeyword(subnode.val()))
        .restriction(typeRestrictions);
  case ConstNode_ConstType_constTypeClass: {
    string className = subnode.val();
    if (className.find("class ") == 0) {
      className = className.substr(6);
    }
    return ObjectTypeSet(classType, false, new ConstantClass(className))
        .restriction(typeRestrictions);
  }

  case ConstNode_ConstType_constTypeVector:
    return ObjectTypeSet(persistentVectorType).restriction(typeRestrictions);
  case ConstNode_ConstType_constTypeMap:
    return ObjectTypeSet(persistentArrayMapType).restriction(typeRestrictions);
  // case ConstNode_ConstType_constTypeSet:
  //   return
  //   ObjectTypeSet(persistentHashSetType).restriction(typeRestrictions);
  case ConstNode_ConstType_constTypeSeq:
    return ObjectTypeSet(persistentListType).restriction(typeRestrictions);
  case ConstNode_ConstType_constTypeVar:
    return ObjectTypeSet(varType).restriction(typeRestrictions);
  default:
    throwCodeGenerationException(
        string("Compiler does not support the following const type yet: ") +
            ConstNode_ConstType_Name(subnode.type()),
        node);
  }
  return ObjectTypeSet();
}
} // namespace rt
