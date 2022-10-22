#include "../codegen.h"  

using namespace std;
using namespace llvm;
using namespace llvm::orc;
using namespace clojure::rt::protobuf::bytecode;

TypedValue CodeGenerator::codegen(const Node &node, const ConstNode &subnode) {
  switch(subnode.type()) {
  case ConstNode_ConstType_constTypeNumber:
    if (node.tag() == "long" || node.otag() == "long" || node.tag() == "class java.lang.Long") {
      return TypedValue(integerType, ConstantInt::get(*TheContext, APInt(64, StringRef(subnode.val().c_str()), 10)));
    } 

    if (node.tag() == "double" || node.otag() == "double" || node.tag() == "class java.lang.Double") {
      return TypedValue(doubleType, ConstantFP::get(*TheContext, 
                             APFloat(APFloatBase::EnumToSemantics
                                     (llvm::APFloatBase::Semantics::S_IEEEdouble), 
                                     StringRef(subnode.val().c_str()))));
    } 


    throw CodeGenerationException(string("Compiler only supports 64 bit integers at this time. "), node);

    break;
  case ConstNode_ConstType_constTypeNil:
    return TypedValue(nilType, dynamicNil());
    break;
  case ConstNode_ConstType_constTypeBool:      
    return TypedValue(booleanType, ConstantInt::getSigned(llvm::Type::getInt1Ty(*TheContext), subnode.val() == "true" ? 1 : 0));
  case ConstNode_ConstType_constTypeString:
    return TypedValue(stringType, dynamicString(subnode.val().c_str()));
  case ConstNode_ConstType_constTypeKeyword:
  case ConstNode_ConstType_constTypeSymbol:
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
  return TypedValue(integerType, nullptr);
}

