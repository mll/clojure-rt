#include "codegen.h"  
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <sstream>
#include "static/Class.h"

using namespace std;
using namespace llvm;

CodeGenerator::CodeGenerator(std::shared_ptr<ProgrammeState> programme, ClojureJIT *weakJIT): TheJIT(weakJIT) {
  TheContext = std::make_unique<LLVMContext>();
  TheModule = std::make_unique<Module>("Clojure JIT", *TheContext);

  TheModule->setDataLayout(TheJIT->getDataLayout());
  
  // Create a new builder for the module.
  Builder = std::make_unique<IRBuilder<>>(*TheContext);
  TheProgramme = programme;
}


void CodeGenerator::printDebugValue(Value *v) {  
  Type *t = v->getType();
  string pf ;
  if (Type::getInt8Ty(*TheContext) == t) pf = "printChar";
  if (Type::getInt64Ty(*TheContext) == t) pf = "printInt";
  if (Type::getInt8Ty(*TheContext)->getPointerTo() == t) pf = "printPointer";
  assert(pf.length() > 0);

  Function *p1 = TheModule->getFunction(pf); 
  if(!p1) {
    vector<Type *> p1at;
    p1at.push_back(t);
    FunctionType *p1t = FunctionType::get(Type::getVoidTy(*TheContext), p1at, false);
    
    p1 = Function::Create(p1t, Function::ExternalLinkage, pf, TheModule.get());
  }
  vector<Value *> p1av;
  p1av.push_back(v);
  Builder->CreateCall(p1, p1av);
}

string CodeGenerator::codegenTopLevel(const Node &node, int i) {
    string fname = string("__anon__") + to_string(i);
    std::vector<Type*> args;
    FunctionType *FT = FunctionType::get(Type::getInt8Ty(*TheContext)->getPointerTo(), args, false);
    Function *F = Function::Create(FT, Function::ExternalLinkage, fname, TheModule.get());
    BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", F);
    Builder->SetInsertPoint(BB);
    Builder->CreateRet(box(codegen(node, ObjectTypeSet::all())).second);
    verifyFunction(*F);
    return fname;
}

void CodeGenerator::codegenDynamicMemoryGuidance(const Node &node) {
  for(int i=0; i<node.dropmemory_size(); i++) {
    auto guidance = node.dropmemory(i);
    dynamicMemoryGuidance(guidance);
  }
}

TypedValue CodeGenerator::codegen(const Node &node, const ObjectTypeSet &typeRestrictions) {
  codegenDynamicMemoryGuidance(node);

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
  case opDeftype:
    return codegen(node, node.subnode().deftype(), typeRestrictions);
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


string CodeGenerator::globalNameForVar(string var) {
  int index;
  while((index = var.find("/")) != string::npos) {    
      var.replace(index, string("_").length(), string("_"));
  }

  while((index = var.find("-")) != string::npos) {    
    var.replace(index, string("-").length(), string("__1__")); 
  }

  while((index = var.find("?")) != string::npos) {    
    var.replace(index, string("?").length(), string("__2__")); 
  }

  while((index = var.find(".")) != string::npos) {    
    var.replace(index, string(".").length(), string("__3__")); 
  }

  return string("cv_") + var;
}


TypedValue CodeGenerator::staticFalse() { 
  return TypedValue(ObjectTypeSet(booleanType, false, new ConstantBoolean(false)), ConstantInt::getSigned(llvm::Type::getInt1Ty(*TheContext), 0));
}

TypedValue CodeGenerator::staticTrue() { 
  return TypedValue(ObjectTypeSet(booleanType, false,  new ConstantBoolean(true)), ConstantInt::getSigned(llvm::Type::getInt1Ty(*TheContext), 1));
}

string CodeGenerator::pointerName(void *ptr) {
  std::stringstream ss;
  ss << ptr;  
  return ss.str();
}

ObjectTypeSet CodeGenerator::typeForArgString(const Node &node, const string &typeString) {
  auto s = typeString.size();
  if(s == 0) throw CodeGenerationException(string("Unknown type code: ")+ typeString, node);
  auto currentChar = typeString[0];
  if (currentChar == 'L') {
    if(s == 1) throw CodeGenerationException(string("Unknown type code: ")+ typeString, node);
    string typeName(&typeString[1]);      
    if (typeName == "D") return ObjectTypeSet(doubleType, true);
    if (typeName == "J") return ObjectTypeSet(integerType, true);
    if (typeName == "Z") return ObjectTypeSet(booleanType, true);
    if (typeName == "S") return ObjectTypeSet(stringType);
    if (typeName == "O") return ObjectTypeSet::all();
    if (typeName == "L") return ObjectTypeSet(persistentListType);
    if (typeName == "V") return ObjectTypeSet(persistentVectorType);
    if (typeName == "N") return ObjectTypeSet(nilType);
    if (typeName == "Y") return ObjectTypeSet(symbolType);
    if (typeName == "C") return ObjectTypeSet(classType);
    if (typeName == "T") return ObjectTypeSet(deftypeType);
    if (typeName == "K") return ObjectTypeSet(keywordType);
    if (typeName == "F") return ObjectTypeSet(functionType);
    if (typeName == "Q") return ObjectTypeSet(varType);
    if (typeName == "I") return ObjectTypeSet(bigIntegerType);
    if (typeName == "R") return ObjectTypeSet(ratioType);
    if (typeName == "A") return ObjectTypeSet(persistentArrayMapType);

    throw CodeGenerationException(string("Unknown class: ")+ typeName + string(" Full string: ") + typeString, node);
  }
  if (currentChar == 'C') return ObjectTypeSet(std::stoi(typeString.substr(1)));
  if (currentChar == 'D') return ObjectTypeSet(doubleType);
  if (currentChar == 'J') return ObjectTypeSet(integerType);
  if (currentChar == 'Z') return ObjectTypeSet(booleanType);
  throw CodeGenerationException(string("Unknown type code: ")+ currentChar + string(" Full string: ") + typeString, node);
}

vector<ObjectTypeSet> CodeGenerator::typesForArgString(const Node &node, const string &typeString) {
  int i = 0;
  vector<ObjectTypeSet> types;
  while(i < typeString.length()) {
    char currentChar[2];
    currentChar[1] = 0;
    currentChar[0] = typeString[i++];
    if (currentChar[0] == 'L') {
      currentChar[0] = typeString[i++];
      string typeName(currentChar);      
      types.push_back(typeForArgString(node, string("L") + typeName));
      continue;
    } else if (currentChar[0] == 'C') { // CclassName;
      int j = i;
      while (typeString[j] != ';') ++j;
      types.push_back(typeForArgString(node, string("C") + typeString.substr(i, j)));
      i = ++j;
      continue;
    }
    types.push_back(typeForArgString(node, string(currentChar)));
  }
  return types;
}

string CodeGenerator::getMangledUniqueFunctionName(uint64_t num) {
  return string("fn_") + to_string(num);
}

uint64_t  CodeGenerator::getUniqueFunctionIdFromName(string name) {
  return stoul(name.substr(3));
}


uint64_t CodeGenerator::getUniqueFunctionId() {
  /* TODO - this might require threadsafe precautions, like std::Atomic */
  return ++TheProgramme->lastFunctionUniqueId;
}

uint64_t CodeGenerator::getUniqueClassId() {
  return TheProgramme->getUniqueClassId();
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
  case opDeftype:
    return getType(node, node.subnode().deftype(), typeRestrictions);
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
