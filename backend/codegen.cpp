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

vector<TypedValue > CodeGenerator::codegen(const Programme &programme) {
  vector<TypedValue> values;
  for (int i=0; i< programme.nodes_size(); i++) {
    auto node = programme.nodes(i);
    values.push_back(codegen(node));
  }
  return values;
}

TypedValue CodeGenerator::codegen(const Node &node) {
  switch (node.op()) {
  case opBinding:
    return codegen(node, node.subnode().binding());    
  case opCase:
    return codegen(node, node.subnode().case_());    
  case opCaseTest:
    return codegen(node, node.subnode().casetest());    
  case opCaseThen:
    return codegen(node, node.subnode().casethen());    
  case opCatch:
    return codegen(node, node.subnode().catch_());    
  case opConst:
    return codegen(node, node.subnode().const_());    
  case opDef:
    return codegen(node, node.subnode().def());    
  case opDo:
    return codegen(node, node.subnode().do_());    
  case opFn:
    return codegen(node, node.subnode().fn());    
  case opFnMethod:
    return codegen(node, node.subnode().fnmethod());    
  case opHostInterop:
    return codegen(node, node.subnode().hostinterop());    
  case opIf:
    return codegen(node, node.subnode().if_());    
  case opImport:
    return codegen(node, node.subnode().import());    
  case opInstanceCall:
    return codegen(node, node.subnode().instancecall());    
  case opInstanceField:
    return codegen(node, node.subnode().instancefield());    
  case opIsInstance:
    return codegen(node, node.subnode().isinstance());    
  case opInvoke:
    return codegen(node, node.subnode().invoke());    
  case opKeywordInvoke:
    return codegen(node, node.subnode().keywordinvoke());    
  case opLet:
    return codegen(node, node.subnode().let());    
  case opLetfn:
    return codegen(node, node.subnode().letfn());    
  case opLocal:
    return codegen(node, node.subnode().local());    
  case opLoop:
    return codegen(node, node.subnode().loop());    
  case opMap:
    return codegen(node, node.subnode().map());    
  case opMethod:
    return codegen(node, node.subnode().method());    
  case opMonitorEnter:
    return codegen(node, node.subnode().monitorenter());    
  case opMonitorExit:
    return codegen(node, node.subnode().monitorexit());    
  case opNew:
    return codegen(node, node.subnode().new_());    
  case opPrimInvoke:
    return codegen(node, node.subnode().priminvoke());    
  case opProtocolInvoke:
    return codegen(node, node.subnode().protocolinvoke());    
  case opQuote:
    return codegen(node, node.subnode().quote());    
  case opRecur:
    return codegen(node, node.subnode().recur());    
  case opReify:
    return codegen(node, node.subnode().reify());    
  case opSet:
    return codegen(node, node.subnode().set());    
  case opMutateSet:
    return codegen(node, node.subnode().mutateset());    
  case opStaticCall:
    return codegen(node, node.subnode().staticcall());    
  case opStaticField:
    return codegen(node, node.subnode().staticfield());    
  case opTheVar:
    return codegen(node, node.subnode().thevar());    
  case opThrow:
    return codegen(node, node.subnode().throw_());    
  case opTry:
    return codegen(node, node.subnode().try_());    
  case opVar:
    return codegen(node, node.subnode().var());    
  case opVector:
    return codegen(node, node.subnode().vector());    
  case opWithMeta:
    return codegen(node, node.subnode().withmeta());
  default:
    throw CodeGenerationException(string("Compiler does not support the following op yet: ") + Op_Name(node.op()), node);
  }
}

string CodeGenerator::typeStringForArgs(vector<TypedValue> &args) {
  stringstream retval;
  for (auto i: args) {
    switch (i.first) {
      case integerType:
        retval << "J";
        break;
      case stringType:
        retval << "Ljava/lang/String;";
        break;
      case persistentListType:
        retval << "Lclojure/lang/PersistentList;";
        break;
      case persistentVectorType:
        retval << "Lclojure/lang/PersistentVector;";
        break;
      case persistentVectorNodeType:
        retval << "Lclojure/lang/PersistentVectorNode;";
        break;
      case doubleType:
        retval << "D";
        break;
    }
  }
  return retval.str();
}

vector<objectType> CodeGenerator::typesForArgString(Node &node, string &typeString) {
  int i = 0;
  vector<objectType> types;
  while(i < typeString.length()) {
    auto currentChar = typeString[i++];
    if (currentChar == 'L') {
      string typeName;
      for(;i < typeString.length() && typeString[i] != ';'; i++) typeName.push_back(typeString[i]);
      if (typeString[i] != ';') throw CodeGenerationException(string("Wrong type string: ")+ typeString, node);
      i++;
      if (typeName == "java/lang/String") { types.push_back(stringType); continue; }
      if (typeName == "clojure/lang/PersistentList") { types.push_back(persistentListType); continue; }
      if (typeName == "clojure/lang/PersistentVector") { types.push_back(persistentVectorType); continue; }
      if (typeName == "clojure/lang/PersistentVectorNode") { types.push_back(persistentVectorNodeType); continue; }
      throw CodeGenerationException(string("Unknown class: ")+ typeName, node);
    }
    if (currentChar == 'D') { types.push_back(doubleType); continue; }
    if (currentChar == 'J') { types.push_back(integerType); continue; }
    throw CodeGenerationException(string("Unknown type code: ")+ currentChar, node);
  }
  return types;
}


 
CodeGenerationException::CodeGenerationException(const string &errorMessage, const Node& node) {
  stringstream retval;
  auto env = node.env();
  cout << env.file() <<  ":" << env.line() << ":" << env.column() << ": error: " << errorMessage << "\n";
  cout << node.form() << "\n";
  cout << string(node.form().length(), '^') << "\n";
  this->errorMessage = retval.str();
}


string CodeGenerationException::toString() {
  return errorMessage;
}


