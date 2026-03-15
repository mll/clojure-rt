#include "CodeGen.h"
#include "../bridge/Exceptions.h"
#include "../cljassert.h"
#include "CleanupChainGuard.h"
#include "TypedValue.h"
#include "runtime/Object.h"
#include "runtime/RTValue.h"
#include "runtime/String.h"

using namespace llvm;
using namespace clojure::rt::protobuf::bytecode;

namespace rt {

CodeGen::~CodeGen() {
  for (auto &val : generatedConstants) {
    Ptr_release(RT_unboxPtr(val));
  }
}

CodeGenResult CodeGen::release() && {
  DIB->finalize();
  auto constants = std::move(generatedConstants);
  generatedConstants.clear(); // Ensure destructor doesn't re-release
  return {std::move(TSContext), std::move(TheModule), std::move(constants)};
}

std::string CodeGen::codegenTopLevel(const Node &node) {
  CLJ_ASSERT(TSContext != nullptr, "Codegen was moved");
  uword_t i = compilerState.functionAstRegistry.registerObject(&node);
  std::string fname = std::string("__repl__") + std::to_string(i);
  FunctionType *FT = FunctionType::get(types.i64Ty, {}, false);
  Function *F =
      Function::Create(FT, Function::ExternalLinkage, fname, *TheModule);

  // Enforce frame pointers for reliable stack traces
  F->addFnAttr("frame-pointer", "all");

  // Create debug info for the function
  auto env = node.env();
  std::string fileName = env.file();
  std::string dir = ".";
  if (fileName.empty()) {
    fileName = CU->getFilename().str();
    dir = CU->getDirectory().str();
  } else {
    size_t lastSlash = fileName.find_last_of('/');
    if (lastSlash != std::string::npos) {
      dir = fileName.substr(0, lastSlash);
      fileName = fileName.substr(lastSlash + 1);
    }
  }

  llvm::DIFile *Unit = DIB->createFile(fileName, dir);
  llvm::DISubroutineType *AsmSignature =
      DIB->createSubroutineType(DIB->getOrCreateTypeArray({}));
  llvm::DISubprogram *SP = DIB->createFunction(
      Unit, fname, fname, Unit, env.line(), AsmSignature, env.line(),
      llvm::DINode::FlagPrototyped, llvm::DISubprogram::SPFlagDefinition);
  F->setSubprogram(SP);
  LexicalBlocks.push_back(SP);

  BasicBlock *BB = BasicBlock::Create(TheContext, "entry", F);
  Builder.SetInsertPoint(BB);

  memoryManagement.initFunction(F);

  // Declare personality function
  FunctionType *personalityFnTy = FunctionType::get(types.i32Ty, true);
  personalityFn =
      TheModule->getOrInsertFunction("__gxx_personality_v0", personalityFnTy);
  F->setPersonalityFn(cast<Function>(personalityFn.getCallee()));

  // Set initial debug location
  Builder.SetCurrentDebugLocation(
      llvm::DILocation::get(TheContext, env.line(), env.column(), SP));

  Builder.CreateRet(
      valueEncoder.box(codegen(node, ObjectTypeSet::all())).value);

  LexicalBlocks.pop_back();
  verifyFunction(*F);
  return fname;
}

TypedValue CodeGen::codegen(const Node &node,
                            const ObjectTypeSet &typeRestrictions) {
  CLJ_ASSERT(TSContext != nullptr, "Codegen was moved");

  MemoryManagement::UnwindGuidanceGuard guidanceGuard(memoryManagement,
                                                      &node.unwindmemory());

  auto env = node.env();
  if (!LexicalBlocks.empty()) {
    Builder.SetCurrentDebugLocation(llvm::DILocation::get(
        TheContext, env.line(), env.column(), LexicalBlocks.back()));
  }

  for (int i = 0; i < node.dropmemory_size(); i++) {
    auto guidance = node.dropmemory(i);
    memoryManagement.dynamicMemoryGuidance(guidance);
  }

  switch (node.op()) {
  case opConst:
    return codegen(node, node.subnode().const_(), typeRestrictions);
  case opQuote:
    return codegen(node, node.subnode().quote(), typeRestrictions);
  case opMap:
    return codegen(node, node.subnode().map(), typeRestrictions);
  case opVector:
    return codegen(node, node.subnode().vector(), typeRestrictions);
  case opStaticCall:
    return codegen(node, node.subnode().staticcall(), typeRestrictions);

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
  case opDef:
    return codegen(node, node.subnode().def(), typeRestrictions);
  // case opDeftype:
  //   return codegen(node, node.subnode().deftype(), typeRestrictions);
  case opDo:
    return codegen(node, node.subnode().do_(), typeRestrictions);
  // case opFn:
  //   return codegen(node, node.subnode().fn(), typeRestrictions);
  // case opFnMethod:
  //   return codegen(node, node.subnode().fnmethod(), typeRestrictions);
  case opHostInterop:
    return codegen(node, node.subnode().hostinterop(), typeRestrictions);
  case opIf:
    return codegen(node, node.subnode().if_(), typeRestrictions);
  // case opImport:
  //   return codegen(node, node.subnode().import(), typeRestrictions);
  case opInstanceCall:
    return codegen(node, node.subnode().instancecall(), typeRestrictions);
  // case opInstanceField:
  //   return codegen(node, node.subnode().instancefield(), typeRestrictions);
  // case opIsInstance:
  //   return codegen(node, node.subnode().isinstance(), typeRestrictions);
  // case opInvoke:
  //   return codegen(node, node.subnode().invoke(), typeRestrictions);
  // case opKeywordInvoke:
  //   return codegen(node, node.subnode().keywordinvoke(), typeRestrictions);
  case opLet:
    return codegen(node, node.subnode().let(), typeRestrictions);
  // case opLetfn:
  //   return codegen(node, node.subnode().letfn(), typeRestrictions);
  case opLocal:
    return codegen(node, node.subnode().local(), typeRestrictions);
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
  case opStaticField:
    return codegen(node, node.subnode().staticfield(), typeRestrictions);
  case opTheVar:
    return codegen(node, node.subnode().thevar(), typeRestrictions);
  // case opThrow:
  //   return codegen(node, node.subnode().throw_(), typeRestrictions);
  // case opTry:
  //   return codegen(node, node.subnode().try_(), typeRestrictions);
  case opVar:
    return codegen(node, node.subnode().var(), typeRestrictions);
  case opWithMeta:
    return codegen(node, node.subnode().withmeta(), typeRestrictions);
  default:
    throwCodeGenerationException(
        std::string("Compiler does not support the following op yet: ") +
            Op_Name(node.op()),
        node);
  }
  return TypedValue(ObjectTypeSet::all(), nullptr);
}

ObjectTypeSet CodeGen::getType(const Node &node,
                               const ObjectTypeSet &typeRestrictions) {
  CLJ_ASSERT(TSContext != nullptr, "Codegen was moved");
  switch (node.op()) {
  case opConst:
    return getType(node, node.subnode().const_(), typeRestrictions);
  case opQuote:
    return getType(node, node.subnode().quote(), typeRestrictions);
  case opVector:
    return getType(node, node.subnode().vector(), typeRestrictions);
  case opMap:
    return getType(node, node.subnode().map(), typeRestrictions);
  case opStaticCall:
    return getType(node, node.subnode().staticcall(), typeRestrictions);

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
  case opDef:
    return getType(node, node.subnode().def(), typeRestrictions);
  // case opDeftype:
  //   return getType(node, node.subnode().deftype(), typeRestrictions);
  case opDo:
    return getType(node, node.subnode().do_(), typeRestrictions);
  // case opFn:
  //   return getType(node, node.subnode().fn(), typeRestrictions);
  // case opFnMethod:
  //   return getType(node, node.subnode().fnmethod(), typeRestrictions);
  case opHostInterop:
    return getType(node, node.subnode().hostinterop(), typeRestrictions);
  case opIf:
    return getType(node, node.subnode().if_(), typeRestrictions);
  // case opImport:
  //   return getType(node, node.subnode().import(), typeRestrictions);
  case opInstanceCall:
    return getType(node, node.subnode().instancecall(), typeRestrictions);
  // case opInstanceField:
  //   return getType(node, node.subnode().instancefield(), typeRestrictions);
  // case opIsInstance:
  //   return getType(node, node.subnode().isinstance(), typeRestrictions);
  // case opInvoke:
  //   return getType(node, node.subnode().invoke(), typeRestrictions);
  // case opKeywordInvoke:
  //   return getType(node, node.subnode().keywordinvoke(), typeRestrictions);
  case opLet:
    return getType(node, node.subnode().let(), typeRestrictions);
  // case opLetfn:
  //   return getType(node, node.subnode().letfn(), typeRestrictions);
  case opLocal:
    return getType(node, node.subnode().local(), typeRestrictions);
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
  case opStaticField:
    return getType(node, node.subnode().staticfield(), typeRestrictions);
  case opTheVar:
    return getType(node, node.subnode().thevar(), typeRestrictions);
  // case opThrow:
  //   return getType(node, node.subnode().throw_(), typeRestrictions);
  // case opTry:
  //   return getType(node, node.subnode().try_(), typeRestrictions);
  case opVar:
    return getType(node, node.subnode().var(), typeRestrictions);
  case opWithMeta:
    return getType(node, node.subnode().withmeta(), typeRestrictions);
  default:
    throwCodeGenerationException(
        std::string("Compiler does not support the following op yet: ") +
            Op_Name(node.op()),
        node);
  }
  return ObjectTypeSet::all();
}

Var *CodeGen::getOrCreateVar(std::string_view name) {
  return compilerState.varRegistry.getOrCreate(
      std::string(name).c_str(), [name]() {
        RTValue keyword =
            Keyword_create(String_createDynamicStr(std::string(name).c_str()));
        return Var_create(keyword);
      });
}

bool CodeGen::canThrow(const Node &node) {
  switch (node.op()) {
  case opConst:
  case opQuote:
  case opStaticField:
  case opTheVar:
  case opLocal:
  case opFn:
  case opFnMethod:
    return false;

  case opLet: {
    const auto &l = node.subnode().let();
    for (int i = 0; i < l.bindings_size(); ++i) {
      if (canThrow(l.bindings(i)))
        return true;
    }
    return canThrow(l.body());
  }

  case opVector: {
    const auto &v = node.subnode().vector();
    for (int i = 0; i < v.items_size(); ++i) {
      if (canThrow(v.items(i)))
        return true;
    }
    return false;
  }

  case opMap: {
    const auto &m = node.subnode().map();
    for (int i = 0; i < m.keys_size(); ++i) {
      if (canThrow(m.keys(i)) || canThrow(m.vals(i)))
        return true;
    }
    return false;
  }

  case opSet: {
    const auto &s = node.subnode().set();
    for (int i = 0; i < s.items_size(); ++i) {
      if (canThrow(s.items(i)))
        return true;
    }
    return false;
  }

  case opIf: {
    const auto &if_ = node.subnode().if_();
    if (canThrow(if_.test()))
      return true;
    if (canThrow(if_.then()))
      return true;
    if (if_.has_else_() && canThrow(if_.else_()))
      return true;
    return false;
  }

  case opDo: {
    const auto &d = node.subnode().do_();
    for (int i = 0; i < d.statements_size(); ++i) {
      if (canThrow(d.statements(i)))
        return true;
    }
    return canThrow(d.ret());
  }

  case opDef: {
    const auto &d = node.subnode().def();
    if (d.has_init() && canThrow(d.init()))
      return true;
    return false;
  }

  case opWithMeta: {
    const auto &wm = node.subnode().withmeta();
    if (canThrow(wm.expr()) || canThrow(wm.meta()))
      return true;
    return false;
  }

  case opStaticCall:
  case opHostInterop:
    return true;

  default:
    // Safer to assume it can throw if we don't know the node type
    return true;
  }
}

} // namespace rt
