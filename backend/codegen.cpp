#include "codegen.h"  
#include "Numbers.h"

CodeGenerator::CodeGenerator() {
  TheContext = std::make_unique<LLVMContext>();
  TheModule = std::make_unique<Module>("Clojure JIT", *TheContext);
  TheJIT = std::move(ClojureJIT::Create().get());

  TheModule->setDataLayout(TheJIT->getDataLayout());
  
  // Create a new builder for the module.
  Builder = std::make_unique<IRBuilder<>>(*TheContext);
  
  // Create a new pass manager attached to it.
  TheFPM = std::make_unique<legacy::FunctionPassManager>(TheModule.get());
  
  // Do simple "peephole" optimizations and bit-twiddling optzns.
  TheFPM->add(createInstructionCombiningPass());
  // Reassociate expressions.
  TheFPM->add(createReassociatePass());
  // Eliminate Common SubExpressions.
  TheFPM->add(createGVNPass());
  // Simplify the control flow graph (deleting unreachable blocks, etc).
  TheFPM->add(createCFGSimplificationPass());
  
  TheFPM->doInitialization();

  auto numbers = getNumbersStaticFunctions();
  StaticCallLibrary.insert(numbers.begin(), numbers.end());
}

vector<TypedValue> CodeGenerator::codegen(const Programme &programme) {
  vector<TypedValue> values;
  for (int i=0; i< programme.nodes_size(); i++) {
    auto node = programme.nodes(i);
    /* TODO: This is all temporary. */
    string fname = string("__anon__") + to_string(i);
    std::vector<Type*> args;
    FunctionType *FT = FunctionType::get(Type::getInt8Ty(*TheContext)->getPointerTo(), args, false);
    Function *F = Function::Create(FT, Function::ExternalLinkage, fname, TheModule.get());
    BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", F);
    Builder->SetInsertPoint(BB);
    Builder->CreateRet(box(codegen(node, ObjectTypeSet::all())));
    verifyFunction(*F);
    //F->print(errs());
    //fprintf(stderr, "\n");
    
    // Remove the anonymous expression.
   // F->eraseFromParent();
  }
  return values;
}

TypedValue CodeGenerator::codegen(const Node &node, const ObjectTypeSet &typeRestrictions) {
  switch (node.op()) {
  case opBinding:
    return codegen(node, node.subnode().binding(), typeRestrictions);    
  case opCase:
    return codegen(node, node.subnode().case_(), typeRestrictions);    
  case opCaseTest:
    return codegen(node, node.subnode().casetest(), typeRestrictions);    
  case opCaseThen:
    return codegen(node, node.subnode().casethen(), typeRestrictions);    
  case opCatch:
    return codegen(node, node.subnode().catch_(), typeRestrictions);    
  case opConst:
    return codegen(node, node.subnode().const_(), typeRestrictions);    
  case opDef:
    return codegen(node, node.subnode().def(), typeRestrictions);    
  case opDo:
    return codegen(node, node.subnode().do_(), typeRestrictions);    
  case opFn:
    return codegen(node, node.subnode().fn(), typeRestrictions);    
  case opFnMethod:
    return codegen(node, node.subnode().fnmethod(), typeRestrictions);    
  case opHostInterop:
    return codegen(node, node.subnode().hostinterop(), typeRestrictions);    
  case opIf:
    return codegen(node, node.subnode().if_(), typeRestrictions);    
  case opImport:
    return codegen(node, node.subnode().import(), typeRestrictions);    
  case opInstanceCall:
    return codegen(node, node.subnode().instancecall(), typeRestrictions);    
  case opInstanceField:
    return codegen(node, node.subnode().instancefield(), typeRestrictions);    
  case opIsInstance:
    return codegen(node, node.subnode().isinstance(), typeRestrictions);    
  case opInvoke:
    return codegen(node, node.subnode().invoke(), typeRestrictions);    
  case opKeywordInvoke:
    return codegen(node, node.subnode().keywordinvoke(), typeRestrictions);    
  case opLet:
    return codegen(node, node.subnode().let(), typeRestrictions);    
  case opLetfn:
    return codegen(node, node.subnode().letfn(), typeRestrictions);    
  case opLocal:
    return codegen(node, node.subnode().local(), typeRestrictions);    
  case opLoop:
    return codegen(node, node.subnode().loop(), typeRestrictions);    
  case opMap:
    return codegen(node, node.subnode().map(), typeRestrictions);    
  case opMethod:
    return codegen(node, node.subnode().method(), typeRestrictions);    
  case opMonitorEnter:
    return codegen(node, node.subnode().monitorenter(), typeRestrictions);    
  case opMonitorExit:
    return codegen(node, node.subnode().monitorexit(), typeRestrictions);    
  case opNew:
    return codegen(node, node.subnode().new_(), typeRestrictions);    
  case opPrimInvoke:
    return codegen(node, node.subnode().priminvoke(), typeRestrictions);    
  case opProtocolInvoke:
    return codegen(node, node.subnode().protocolinvoke(), typeRestrictions);    
  case opQuote:
    return codegen(node, node.subnode().quote(), typeRestrictions);    
  case opRecur:
    return codegen(node, node.subnode().recur(), typeRestrictions);    
  case opReify:
    return codegen(node, node.subnode().reify(), typeRestrictions);    
  case opSet:
    return codegen(node, node.subnode().set(), typeRestrictions);    
  case opMutateSet:
    return codegen(node, node.subnode().mutateset(), typeRestrictions);    
  case opStaticCall:
    return codegen(node, node.subnode().staticcall(), typeRestrictions);    
  case opStaticField:
    return codegen(node, node.subnode().staticfield(), typeRestrictions);    
  case opTheVar:
    return codegen(node, node.subnode().thevar(), typeRestrictions);    
  case opThrow:
    return codegen(node, node.subnode().throw_(), typeRestrictions);    
  case opTry:
    return codegen(node, node.subnode().try_(), typeRestrictions);    
  case opVar:
    return codegen(node, node.subnode().var(), typeRestrictions);    
  case opVector:
    return codegen(node, node.subnode().vector(), typeRestrictions);    
  case opWithMeta:
    return codegen(node, node.subnode().withmeta(), typeRestrictions);
  default:
    throw CodeGenerationException(string("Compiler does not support the following op yet: ") + Op_Name(node.op()), node);
  }
}

string CodeGenerator::typeStringForArgs(const vector<TypedValue> &args) {
  stringstream retval;
  for (auto i: args) {
    switch (i.first.determinedType()) {
      case integerType:
        retval << "J";
        break;
      case stringType:
        retval << "LS";
        break;
      case persistentListType:
        retval << "LL";
        break;
      case persistentVectorType:
        retval << "LV";
        break;
      case symbolType:
        retval << "LY";
        break;
      case persistentVectorNodeType:
        assert(false && "Node cannot be used as an argument");
        break;
      case doubleType:
        retval << "D";
        break;
      case booleanType:
        retval << "B";
        break;
      case nilType:
        retval << "LN";
        break;
    }
  }
  return retval.str();
}

vector<objectType> CodeGenerator::typesForArgString(const Node &node, const string &typeString) {
  int i = 0;
  vector<objectType> types;
  while(i < typeString.length()) {
    auto currentChar = typeString[i++];
    if (currentChar == 'L') {
      string typeName(&typeString[i++]);      
      if (typeName == "S") { types.push_back(stringType); continue; }
      if (typeName == "L") { types.push_back(persistentListType); continue; }
      if (typeName == "V") { types.push_back(persistentVectorType); continue; }
      if (typeName == "N") { types.push_back(nilType); continue; }
      if (typeName == "Y") { types.push_back(symbolType); continue; }
      throw CodeGenerationException(string("Unknown class: ")+ typeName, node);
    }
    if (currentChar == 'D') { types.push_back(doubleType); continue; }
    if (currentChar == 'J') { types.push_back(integerType); continue; }
    if (currentChar == 'Z') { types.push_back(booleanType); continue; }
    throw CodeGenerationException(string("Unknown type code: ")+ currentChar, node);
  }
  return types;
}


 
CodeGenerationException::CodeGenerationException(const string &errorMessage, const Node& node) {
  stringstream retval;
  auto env = node.env();
  retval << env.file() <<  ":" << env.line() << ":" << env.column() << ": error: " << errorMessage << "\n";
  retval << node.form() << "\n";
  retval << string(node.form().length(), '^') << "\n";
  this->errorMessage = retval.str();
}


string CodeGenerationException::toString() const {
  return errorMessage;
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const ObjectTypeSet &typeRestrictions) {
  switch (node.op()) {
  case opBinding:
    return getType(node, node.subnode().binding(), typeRestrictions);    
  case opCase:
    return getType(node, node.subnode().case_(), typeRestrictions);    
  case opCaseTest:
    return getType(node, node.subnode().casetest(), typeRestrictions);    
  case opCaseThen:
    return getType(node, node.subnode().casethen(), typeRestrictions);    
  case opCatch:
    return getType(node, node.subnode().catch_(), typeRestrictions);    
  case opConst:
    return getType(node, node.subnode().const_(), typeRestrictions);    
  case opDef:
    return getType(node, node.subnode().def(), typeRestrictions);    
  case opDo:
    return getType(node, node.subnode().do_(), typeRestrictions);    
  case opFn:
    return getType(node, node.subnode().fn(), typeRestrictions);    
  case opFnMethod:
    return getType(node, node.subnode().fnmethod(), typeRestrictions);    
  case opHostInterop:
    return getType(node, node.subnode().hostinterop(), typeRestrictions);    
  case opIf:
    return getType(node, node.subnode().if_(), typeRestrictions);    
  case opImport:
    return getType(node, node.subnode().import(), typeRestrictions);    
  case opInstanceCall:
    return getType(node, node.subnode().instancecall(), typeRestrictions);    
  case opInstanceField:
    return getType(node, node.subnode().instancefield(), typeRestrictions);    
  case opIsInstance:
    return getType(node, node.subnode().isinstance(), typeRestrictions);    
  case opInvoke:
    return getType(node, node.subnode().invoke(), typeRestrictions);    
  case opKeywordInvoke:
    return getType(node, node.subnode().keywordinvoke(), typeRestrictions);    
  case opLet:
    return getType(node, node.subnode().let(), typeRestrictions);    
  case opLetfn:
    return getType(node, node.subnode().letfn(), typeRestrictions);    
  case opLocal:
    return getType(node, node.subnode().local(), typeRestrictions);    
  case opLoop:
    return getType(node, node.subnode().loop(), typeRestrictions);    
  case opMap:
    return getType(node, node.subnode().map(), typeRestrictions);    
  case opMethod:
    return getType(node, node.subnode().method(), typeRestrictions);    
  case opMonitorEnter:
    return getType(node, node.subnode().monitorenter(), typeRestrictions);    
  case opMonitorExit:
    return getType(node, node.subnode().monitorexit(), typeRestrictions);    
  case opNew:
    return getType(node, node.subnode().new_(), typeRestrictions);    
  case opPrimInvoke:
    return getType(node, node.subnode().priminvoke(), typeRestrictions);    
  case opProtocolInvoke:
    return getType(node, node.subnode().protocolinvoke(), typeRestrictions);    
  case opQuote:
    return getType(node, node.subnode().quote(), typeRestrictions);    
  case opRecur:
    return getType(node, node.subnode().recur(), typeRestrictions);    
  case opReify:
    return getType(node, node.subnode().reify(), typeRestrictions);    
  case opSet:
    return getType(node, node.subnode().set(), typeRestrictions);    
  case opMutateSet:
    return getType(node, node.subnode().mutateset(), typeRestrictions);    
  case opStaticCall:
    return getType(node, node.subnode().staticcall(), typeRestrictions);    
  case opStaticField:
    return getType(node, node.subnode().staticfield(), typeRestrictions);    
  case opTheVar:
    return getType(node, node.subnode().thevar(), typeRestrictions);    
  case opThrow:
    return getType(node, node.subnode().throw_(), typeRestrictions);    
  case opTry:
    return getType(node, node.subnode().try_(), typeRestrictions);    
  case opVar:
    return getType(node, node.subnode().var(), typeRestrictions);    
  case opVector:
    return getType(node, node.subnode().vector(), typeRestrictions);    
  case opWithMeta:
    return getType(node, node.subnode().withmeta(), typeRestrictions);
  default:
    throw CodeGenerationException(string("Compiler does not support the following op yet: ") + Op_Name(node.op()), node);
  }
}

