#include "CodeGen.h"
#include "../bridge/Exceptions.h"
#include "TypedValue.h"

namespace rt {
  TypedValue CodeGen::codegen(const Node &node,
                                    const ObjectTypeSet &typeRestrictions) {
    // TODO - memory guidance
    // codegenDynamicMemoryGuidance(node);

    switch (node.op()) {
    case opConst:
      return codegen(node, node.subnode().const_(), typeRestrictions);
    case opQuote:
      return codegen(node, node.subnode().quote(), typeRestrictions);    
    case opMap:
      return codegen(node, node.subnode().map(), typeRestrictions);    
    case opVector:
      return codegen(node, node.subnode().vector(), typeRestrictions);    

      
    // case opBinding:
    //   return codegen(node, node.subnode().binding(), typeRestrictions);    
    // case opCase:
    //   return codegen(node, node.subnode().case_(), typeRestrictions);    
    // case opCaseTest:
    //   return codegen(node, node.subnode().casetest(), typeRestrictions);    
    // case opCaseThen:
    //   return codegen(node, node.subnode().casethen(), typeRestrictions);    
    // case opCatch:
    //   return codegen(node, node.subnode().catch_(), typeRestrictions);    
    // case opDef:
    //   return codegen(node, node.subnode().def(), typeRestrictions);    
    // case opDeftype:
    //   return codegen(node, node.subnode().deftype(), typeRestrictions);
    // case opDo:
    //   return codegen(node, node.subnode().do_(), typeRestrictions);    
    // case opFn:
    //   return codegen(node, node.subnode().fn(), typeRestrictions);    
    // case opFnMethod:
    //   return codegen(node, node.subnode().fnmethod(), typeRestrictions);    
    // case opHostInterop:
    //   return codegen(node, node.subnode().hostinterop(), typeRestrictions);    
    // case opIf:
    //   return codegen(node, node.subnode().if_(), typeRestrictions);    
    // case opImport:
    //   return codegen(node, node.subnode().import(), typeRestrictions);    
    // case opInstanceCall:
    //   return codegen(node, node.subnode().instancecall(), typeRestrictions);    
    // case opInstanceField:
    //   return codegen(node, node.subnode().instancefield(), typeRestrictions);    
    // case opIsInstance:
    //   return codegen(node, node.subnode().isinstance(), typeRestrictions);    
    // case opInvoke:
    //   return codegen(node, node.subnode().invoke(), typeRestrictions);    
    // case opKeywordInvoke:
    //   return codegen(node, node.subnode().keywordinvoke(), typeRestrictions);    
    // case opLet:
    //   return codegen(node, node.subnode().let(), typeRestrictions);    
    // case opLetfn:
    //   return codegen(node, node.subnode().letfn(), typeRestrictions);    
    // case opLocal:
    //   return codegen(node, node.subnode().local(), typeRestrictions);    
    // case opLoop:
    //   return codegen(node, node.subnode().loop(), typeRestrictions);    
    // case opMethod:
    //   return codegen(node, node.subnode().method(), typeRestrictions);    
    // case opMonitorEnter:
    //   return codegen(node, node.subnode().monitorenter(), typeRestrictions);    
    // case opMonitorExit:
    //   return codegen(node, node.subnode().monitorexit(), typeRestrictions);    
    // case opNew:
    //   return codegen(node, node.subnode().new_(), typeRestrictions);    
    // case opPrimInvoke:
    //   return codegen(node, node.subnode().priminvoke(), typeRestrictions);    
    // case opProtocolInvoke:
    //   return codegen(node, node.subnode().protocolinvoke(), typeRestrictions);    
    // case opRecur:
    //   return codegen(node, node.subnode().recur(), typeRestrictions);    
    // case opReify:
    //   return codegen(node, node.subnode().reify(), typeRestrictions);    
    // case opSet:
    //   return codegen(node, node.subnode().set(), typeRestrictions);    
    // case opMutateSet:
    //   return codegen(node, node.subnode().mutateset(), typeRestrictions);    
    // case opStaticCall:
    //   return codegen(node, node.subnode().staticcall(), typeRestrictions);    
    // case opStaticField:
    //   return codegen(node, node.subnode().staticfield(), typeRestrictions);    
    // case opTheVar:
    //   return codegen(node, node.subnode().thevar(), typeRestrictions);    
    // case opThrow:
    //   return codegen(node, node.subnode().throw_(), typeRestrictions);    
    // case opTry:
    //   return codegen(node, node.subnode().try_(), typeRestrictions);    
    // case opVar:
    //   return codegen(node, node.subnode().var(), typeRestrictions);    
    // case opWithMeta:
    //   return codegen(node, node.subnode().withmeta(), typeRestrictions);
    default:
      throwCodeGenerationException(std::string("Compiler does not support the following op yet: ") + Op_Name(node.op()), node);
    }
    return TypedValue(ObjectTypeSet::all(), nullptr);
  }

  ObjectTypeSet CodeGen::getType(const Node &node, const ObjectTypeSet &typeRestrictions) {
    switch (node.op()) {
    case opConst:
      return getType(node, node.subnode().const_(), typeRestrictions);
    case opQuote:
      return getType(node, node.subnode().quote(), typeRestrictions);
    case opVector:
      return getType(node, node.subnode().vector(), typeRestrictions);    
    case opMap:
      return getType(node, node.subnode().map(), typeRestrictions);    

      
    // case opBinding:
    //   return getType(node, node.subnode().binding(), typeRestrictions);    
    // case opCase:
    //   return getType(node, node.subnode().case_(), typeRestrictions);    
    // case opCaseTest:
    //   return getType(node, node.subnode().casetest(), typeRestrictions);    
    // case opCaseThen:
    //   return getType(node, node.subnode().casethen(), typeRestrictions);    
    // case opCatch:
    //   return getType(node, node.subnode().catch_(), typeRestrictions);    
    // case opDef:
    //   return getType(node, node.subnode().def(), typeRestrictions);    
    // case opDeftype:
    //   return getType(node, node.subnode().deftype(), typeRestrictions);
    // case opDo:
    //   return getType(node, node.subnode().do_(), typeRestrictions);    
    // case opFn:
    //   return getType(node, node.subnode().fn(), typeRestrictions);    
    // case opFnMethod:
    //   return getType(node, node.subnode().fnmethod(), typeRestrictions);    
    // case opHostInterop:
    //   return getType(node, node.subnode().hostinterop(), typeRestrictions);    
    // case opIf:
    //   return getType(node, node.subnode().if_(), typeRestrictions);    
    // case opImport:
    //   return getType(node, node.subnode().import(), typeRestrictions);    
    // case opInstanceCall:
    //   return getType(node, node.subnode().instancecall(), typeRestrictions);    
    // case opInstanceField:
    //   return getType(node, node.subnode().instancefield(), typeRestrictions);    
    // case opIsInstance:
    //   return getType(node, node.subnode().isinstance(), typeRestrictions);    
    // case opInvoke:
    //   return getType(node, node.subnode().invoke(), typeRestrictions);    
    // case opKeywordInvoke:
    //   return getType(node, node.subnode().keywordinvoke(), typeRestrictions);    
    // case opLet:
    //   return getType(node, node.subnode().let(), typeRestrictions);    
    // case opLetfn:
    //   return getType(node, node.subnode().letfn(), typeRestrictions);    
    // case opLocal:
    //   return getType(node, node.subnode().local(), typeRestrictions);    
    // case opLoop:
    //   return getType(node, node.subnode().loop(), typeRestrictions);    
    // case opMethod:
    //   return getType(node, node.subnode().method(), typeRestrictions);    
    // case opMonitorEnter:
    //   return getType(node, node.subnode().monitorenter(), typeRestrictions);    
    // case opMonitorExit:
    //   return getType(node, node.subnode().monitorexit(), typeRestrictions);    
    // case opNew:
    //   return getType(node, node.subnode().new_(), typeRestrictions);    
    // case opPrimInvoke:
    //   return getType(node, node.subnode().priminvoke(), typeRestrictions);    
    // case opProtocolInvoke:
    //   return getType(node, node.subnode().protocolinvoke(), typeRestrictions);    
    // case opRecur:
    //   return getType(node, node.subnode().recur(), typeRestrictions);    
    // case opReify:
    //   return getType(node, node.subnode().reify(), typeRestrictions);    
    // case opSet:
    //   return getType(node, node.subnode().set(), typeRestrictions);    
    // case opMutateSet:
    //   return getType(node, node.subnode().mutateset(), typeRestrictions);    
    // case opStaticCall:
    //   return getType(node, node.subnode().staticcall(), typeRestrictions);    
    // case opStaticField:
    //   return getType(node, node.subnode().staticfield(), typeRestrictions);    
    // case opTheVar:
    //   return getType(node, node.subnode().thevar(), typeRestrictions);    
    // case opThrow:
    //   return getType(node, node.subnode().throw_(), typeRestrictions);    
    // case opTry:
    //   return getType(node, node.subnode().try_(), typeRestrictions);    
    // case opVar:
    //   return getType(node, node.subnode().var(), typeRestrictions);    
    // case opWithMeta:
    //   return getType(node, node.subnode().withmeta(), typeRestrictions);
    default:
      throwCodeGenerationException(std::string("Compiler does not support the following op yet: ") + Op_Name(node.op()), node);
    }
    return ObjectTypeSet::all();
  }  
  
} // namespace rt

