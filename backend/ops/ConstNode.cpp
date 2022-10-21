#include "../codegen.h"  

using namespace std;
using namespace llvm;
using namespace llvm::orc;
using namespace clojure::rt::protobuf::bytecode;

pair<objectType, Value *> CodeGenerator::codegen(const Node &node, const ConstNode &subnode) {
 // cout << "Parsing const node" << endl;
 // cout << subnode.type() << endl;
 // cout << subnode.val() << endl;
 // throw CodeGenerationException(string("Compiler does not support the following op yet: ") + Op_Name(node.op()), node);
  
  switch(subnode.type()) {
  case ConstNode_ConstType_constTypeNumber:
    if (node.tag() == "long" || node.otag() == "long" || node.tag() == "class java.lang.Long") {
      return pair<objectType, Value*>(integerType, ConstantInt::get(*TheContext, APInt(64, StringRef(subnode.val().c_str()), 10)));
    } 

    if (node.tag() == "double" || node.otag() == "double" || node.tag() == "class java.lang.Double") {
      return pair<objectType, Value*>(doubleType, ConstantFP::get(*TheContext, 
                             APFloat(APFloatBase::EnumToSemantics
                                     (llvm::APFloatBase::Semantics::S_IEEEdouble), 
                                     StringRef(subnode.val().c_str()))));
    } 


    throw CodeGenerationException(string("Compiler only supports 64 bit integers at this time. "), node);

    break;
  case ConstNode_ConstType_constTypeNil:
  case ConstNode_ConstType_constTypeBool:
  case ConstNode_ConstType_constTypeKeyword:
  case ConstNode_ConstType_constTypeSymbol:
  case ConstNode_ConstType_constTypeString:
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
  return pair<objectType, Value *>(integerType, nullptr);
}

