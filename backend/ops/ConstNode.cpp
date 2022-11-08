#include "../codegen.h"  

using namespace std;
using namespace llvm;
using namespace llvm::orc;
using namespace clojure::rt::protobuf::bytecode;

TypedValue CodeGenerator::codegen(const Node &node, const ConstNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto types = getType(node, subnode, typeRestrictions);
  
  if(types.size() == 0) throw CodeGenerationException(string("This type of constant cannot be inserted because of type mismatch:  ") + typeRestrictions.toString(), node);

  if(!types.isDetermined()) throw CodeGenerationException(string("Constant type must be well defined. Instead we have found a composite type: ") + types.toString(), node);

  Value *retVal;

  auto name = subnode.val();

  switch(types.determinedType()) {
  case integerType:
    retVal = ConstantInt::get(*TheContext, APInt(64, StringRef(subnode.val().c_str()), 10));
    break;
  case doubleType:
    retVal = ConstantFP::get(*TheContext, 
                           APFloat(APFloatBase::EnumToSemantics
                                   (llvm::APFloatBase::Semantics::S_IEEEdouble), 
                                   StringRef(subnode.val().c_str())));
    break;
  case nilType:   
    retVal = dynamicNil();
    break;
  case booleanType:          
    retVal = ConstantInt::getSigned(llvm::Type::getInt1Ty(*TheContext), subnode.val() == "true" ? 1 : 0);
    break;
  case stringType:    
    retVal = dynamicString(subnode.val().c_str());
    break;
  case symbolType:
    retVal = dynamicSymbol(name.c_str());
    break;
  case keywordType:
    retVal = dynamicKeyword((name[0] == ':' ? name.substr(1) : name).c_str());
   
    break;    
  case persistentListType:
  case persistentVectorType:
  case persistentVectorNodeType:
  case concurrentHashMapType:
  case functionType:
    throw CodeGenerationException(string("Compiler does not support the following const type yet: ") + ConstNode_ConstType_Name(subnode.type()), node);
  }
  return TypedValue(types, retVal, false, "", true);
}


ObjectTypeSet CodeGenerator::getType(const Node &node, const ConstNode &subnode, const ObjectTypeSet &typeRestrictions) {
  switch(subnode.type()) {
  case ConstNode_ConstType_constTypeNumber:
    if (node.tag() == "long" || node.otag() == "long" || node.tag() == "class java.lang.Long") {
      return ObjectTypeSet(integerType).intersection(typeRestrictions);
    } 

    if (node.tag() == "double" || node.otag() == "double" || node.tag() == "class java.lang.Double") {
      return ObjectTypeSet(doubleType).intersection(typeRestrictions);
    } 

    throw CodeGenerationException(string("Compiler only supports 64 bit integers at this time. "), node);

    break;
  case ConstNode_ConstType_constTypeNil:
    return ObjectTypeSet(nilType).intersection(typeRestrictions);
  case ConstNode_ConstType_constTypeBool:      
    return ObjectTypeSet(booleanType).intersection(typeRestrictions);
  case ConstNode_ConstType_constTypeString:
    return ObjectTypeSet(stringType).intersection(typeRestrictions);
  case ConstNode_ConstType_constTypeSymbol:
    return ObjectTypeSet(symbolType).intersection(typeRestrictions);
  case ConstNode_ConstType_constTypeKeyword:
    return ObjectTypeSet(keywordType).intersection(typeRestrictions);
  case ConstNode_ConstType_constTypeType:
  case ConstNode_ConstType_constTypeRecord:
  case ConstNode_ConstType_constTypeMap:
  case ConstNode_ConstType_constTypeVector:
  case ConstNode_ConstType_constTypeSet:
  case ConstNode_ConstType_constTypeSeq:
  case ConstNode_ConstType_constTypeChar:
  case ConstNode_ConstType_constTypeRegex:
  case ConstNode_ConstType_constTypeClass:
  case ConstNode_ConstType_constTypeVar:
  case ConstNode_ConstType_constTypeUnknown:
  default:
    throw CodeGenerationException(string("Compiler does not support the following const type yet: ") + ConstNode_ConstType_Name(subnode.type()), node);
  }
  return ObjectTypeSet();
}
