#include "../codegen.h"  

TypedValue CodeGenerator::codegen(const Node &node, const LocalNode &subnode, const ObjectTypeSet &typeRestrictions) {
  switch(subnode.local()) {
  case localTypeArg:
    {      
      auto args = FunctionArgsStack.back();
      return args[subnode.argid()];
    }
    break;
  default:
    // TODO: cases other than args      
      throw CodeGenerationException(string("Compiler does not fully support the following op yet: ") + Op_Name(node.op()), node);   
  }
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const LocalNode &subnode, const ObjectTypeSet &typeRestrictions) {
  switch(subnode.local()) {
    case localTypeArg:      
      if(FunctionArgTypesStack.empty()) {
        auto args = FunctionArgsStack.back();
        return args[subnode.argid()].first;        
      } else {
        auto args = FunctionArgTypesStack.back();
        return args[subnode.argid()];          
      }              
      break;
    default:
      // TODO: cases other than args      
      throw CodeGenerationException(string("Compiler does not fully support the following op yet: ") + Op_Name(node.op()), node);   
  }
}
