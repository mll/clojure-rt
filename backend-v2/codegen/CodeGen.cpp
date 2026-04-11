#include "CodeGen.h"
#include "../bridge/Exceptions.h"
#include "../bridge/SourceLocation.h"
#include "../cljassert.h"
#include "CleanupChainGuard.h"
#include "TypedValue.h"
#include "runtime/Object.h"
#include "runtime/RTValue.h"
#include "runtime/String.h"
#include <random>
#include <sstream>

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
  return {std::move(TSContext), std::move(TheModule), std::move(constants),
          std::move(formMap)};
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

  TypedValue result = codegen(node, ObjectTypeSet::all());
  Builder.CreateRet(valueEncoder.box(result).value);

  LexicalBlocks.pop_back();
  verifyFunction(*F);
  return fname;
}

std::string CodeGen::generateInstanceCallBridge(
    const std::string &moduleName, const std::string &methodName,
    const ObjectTypeSet &instanceType,
    const std::vector<ObjectTypeSet> &argTypes) {
  CLJ_ASSERT(TSContext != nullptr, "Codegen was moved");
  static thread_local std::mt19937 gen(std::random_device{}());
  std::uniform_int_distribution<uint64_t> dist;
  std::stringstream ss;
  ss << std::hex << dist(gen);

  std::string funcName = moduleName + "_" +
                         std::to_string(instanceType.determinedType()) + "_" +
                         ss.str();
  // 1. Define Signature: (Instance, Arg1, ...) -> RTValue
  // Specialized arguments use natural LLVM types for unboxed primitives
  std::vector<llvm::Type *> llvmArgTypes;
  llvmArgTypes.push_back(types.RT_valueTy); // Instance
  for (size_t i = 0; i < argTypes.size(); i++) {
    const auto &t = argTypes[i];
    if (t.isDetermined() && !t.isBoxedType()) {
      uint32_t type = t.determinedType();
      if (type == integerType)
        llvmArgTypes.push_back(types.i32Ty);
      else if (type == doubleType)
        llvmArgTypes.push_back(types.doubleTy);
      else if (type == booleanType)
        llvmArgTypes.push_back(types.i1Ty);
      else if (type == keywordType || type == symbolType || type == nilType)
        llvmArgTypes.push_back(types.RT_valueTy);
      else
        llvmArgTypes.push_back(types.ptrTy);
    } else {
      llvmArgTypes.push_back(types.RT_valueTy);
    }
  }

  llvm::FunctionType *FT =
      llvm::FunctionType::get(types.RT_valueTy, llvmArgTypes, false);
  llvm::Function *F = llvm::Function::Create(
      FT, llvm::Function::ExternalLinkage, funcName, *TheModule);

  // Enforce frame pointers for reliable stack traces
  F->addFnAttr("frame-pointer", "all");

  // Set personality function for exception handling
  FunctionType *personalityFnTy = FunctionType::get(types.i32Ty, true);
  personalityFn =
      TheModule->getOrInsertFunction("__gxx_personality_v0", personalityFnTy);
  F->setPersonalityFn(cast<Function>(personalityFn.getCallee()));

  // 3. Create entry block
  llvm::BasicBlock *BB = llvm::BasicBlock::Create(TheContext, "entry", F);
  Builder.SetInsertPoint(BB);

  memoryManagement.initFunction(F);

  // 4. Prepare TypedValues for the call
  auto it = F->arg_begin();

  // First argument is the instance, specialized to the known type from IC
  TypedValue instanceTV(instanceType.boxed(), it++);

  std::vector<TypedValue> callArgs;
  for (const auto &t : argTypes) {
    // Subsequent arguments use the types provided by the bouncer/call-site
    callArgs.push_back(TypedValue(t, it++));
  }

  // 5. Generate the specialized instance call
  // This will emit direct calls or a dispatch tree based on the provided types.
  TypedValue result =
      invokeManager.generateInstanceCall(methodName, instanceTV, callArgs);

  // 6. Ensure result is boxed and return
  Builder.CreateRet(valueEncoder.box(result).value);

  verifyFunction(*F);
  return funcName;
}

llvm::Function *CodeGen::generateBaselineMethod(
    const FnMethodNode &method,
    const std::vector<std::pair<std::string, ObjectTypeSet>> &captureInfo) {
  CLJ_ASSERT(TSContext != nullptr, "Codegen was moved");
  static thread_local std::mt19937 gen(std::random_device{}());
  std::uniform_int_distribution<uint64_t> dist;
  std::stringstream ss;
  ss << std::hex << dist(gen);

  std::string funcName = "fn_" + ss.str();

  // Create the function with baseline signature: (Frame*, Arg0, Arg1, Arg2,
  // Arg3, Arg4) -> RTValue
  Function *F =
      Function::Create(types.baselineFunctionTy, Function::ExternalLinkage,
                       funcName, *TheModule);

  // Use the guard to save current IR state and memory management context
  FunctionScopeGuard scopeGuard(*this, F);

  // Enforce frame pointers for reliable stack traces
  F->addFnAttr("frame-pointer", "all");

  // Create entry block
  BasicBlock *BB = BasicBlock::Create(TheContext, "entry", F);
  Builder.SetInsertPoint(BB);

  // Unpack arguments from LLVM function
  auto arg_it = F->arg_begin();
  Value *framePtr = &*arg_it++; // Arg 0: Frame*
  Value *regArgs[5];            // Arg 1-5: RTValue registers
  for (int i = 0; i < 5; ++i) {
    regArgs[i] = &*arg_it++;
  }

  // Set personality function for exception handling
  FunctionType *personalityFnTy = FunctionType::get(types.i32Ty, true);
  personalityFn =
      TheModule->getOrInsertFunction("__gxx_personality_v0", personalityFnTy);
  F->setPersonalityFn(cast<Function>(personalityFn.getCallee()));

  // Prepare environment for body codegen
  variableBindingStack.push();
  variableTypesBindingsStack.push();

  int isVariadic = method.isvariadic();
  int numParams = method.params_size();
  Value *nilValue = valueEncoder.boxNil().value;

  // 1. Unpack parameters
  for (int i = 0; i < numParams; ++i) {
    const auto &paramNode = method.params(i);
    std::string paramName = paramNode.subnode().binding().name();

    Value *val = nullptr;

    if (isVariadic && i == numParams - 1) {
      // It's the variadic parameter. Unpack from frame->variadicSeq (index 2)
      Value *variadicSeqPtr = Builder.CreateStructGEP(types.frameTy, framePtr,
                                                      2, "variadicSeq_ptr");
      val = Builder.CreateLoad(types.RT_valueTy, variadicSeqPtr, "variadicSeq");
    } else {
      // Regular parameter
      if (i < 5) {
        // From registers
        val = regArgs[i];
      } else {
        // From frame->locals[i-5] (index 5)
        Value *localsBase =
            Builder.CreateStructGEP(types.frameTy, framePtr, 5, "locals_base");
        // Accessing element i-5 in the flexible array
        Value *argPtr = Builder.CreateInBoundsGEP(
            types.RT_valueTy, localsBase,
            {Builder.getInt32(0), Builder.getInt32(i - 5)}, "arg_ptr");
        val = Builder.CreateLoad(types.RT_valueTy, argPtr, "arg_val");
      }
    }

    if (!val)
      val = nilValue;

    TypedValue paramTV(ObjectTypeSet::dynamicType(), val);
    variableBindingStack.set(paramName, paramTV);
    variableTypesBindingsStack.set(paramName, ObjectTypeSet::dynamicType());
  }

  // 2. Unpack closed overs (captures) with type propagation
  if (!captureInfo.empty()) {
    // a. Get method pointer from frame
    Value *methodPtrPtr =
        Builder.CreateStructGEP(types.frameTy, framePtr, 1, "method_ptr_ptr");
    Value *methodPtr =
        Builder.CreateLoad(types.ptrTy, methodPtrPtr, "method_ptr");

    // b. Load closedOvers array pointer from method
    Value *closedOversPtrPtr = Builder.CreateStructGEP(
        types.methodTy, methodPtr, 6, "closedOvers_ptr_ptr");
    Value *closedOversPtr =
        Builder.CreateLoad(types.ptrTy, closedOversPtrPtr, "closedOvers_ptr");

    // c. Unpack each capture
    for (int i = 0; i < (int)captureInfo.size(); ++i) {
      const std::string &name = captureInfo[i].first;
      const ObjectTypeSet &type = captureInfo[i].second;

      Value *valRawPtr = Builder.CreateInBoundsGEP(
          types.RT_valueTy, closedOversPtr, Builder.getInt64(i), "capture_ptr");
      Value *valRaw =
          Builder.CreateLoad(types.RT_valueTy, valRawPtr, "capture_raw");

      Value *val = valRaw;
      if (!type.isBoxedType()) {
        if (type.isUnboxedType(integerType)) {
          val = valueEncoder.unboxInt32(TypedValue(type.boxed(), valRaw)).value;
        } else if (type.isUnboxedType(doubleType)) {
          val =
              valueEncoder.unboxDouble(TypedValue(type.boxed(), valRaw)).value;
        } else if (type.isUnboxedType(booleanType)) {
          val = valueEncoder.unboxBool(TypedValue(type.boxed(), valRaw)).value;
        }
      }

      variableBindingStack.set(name, TypedValue(type, val));
      variableTypesBindingsStack.set(name, type);
    }
  }

  // Generate body
  TypedValue result = codegen(method.body(), ObjectTypeSet::all());

  // TODO - it is not yet clear if this should always happen.
  // Previous function calls cound introduce the flush as well, we need to
  // carefully analyse the memory model for closed overs.
  //
  // llvm::FunctionType *flushTy =
  // llvm::FunctionType::get(types.voidTy, {}, false);
  // invokeManager.invokeRaw("Ebr_flush_critical", flushTy, {});

  // Box result and return
  Builder.CreateRet(valueEncoder.box(result).value);

  // Clean up bindings
  variableBindingStack.pop();
  variableTypesBindingsStack.pop();

  verifyFunction(*F);
  return F;
}

TypedValue CodeGen::codegen(const Node &node,
                            const ObjectTypeSet &typeRestrictions) {
  CLJ_ASSERT(TSContext != nullptr, "Codegen was moved");

  MemoryManagement::UnwindGuidanceGuard guidanceGuard(memoryManagement,
                                                      &node.unwindmemory());

  auto env = node.env();

  // Record form for exception reporting
  SourceLocation loc;
  loc.file = env.file();
  loc.line = env.line();
  loc.column = env.column();
  formMap[loc] = node.form();

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
  case opIsInstance:
    return codegen(node, node.subnode().isinstance(), typeRestrictions);
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
  case opNew:
    return codegen(node, node.subnode().new_(), typeRestrictions);
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
  case opThrow:
    return this->codegen(
        node,
        static_cast<const clojure::rt::protobuf::bytecode::ThrowNode &>(
            node.subnode().throw_()),
        typeRestrictions);
  case opTry:
  //   return codegen(node, node.subnode().try_(), typeRestrictions);
  case opVar:
    return codegen(node, node.subnode().var(), typeRestrictions);
  case opFn:
    return codegen(node, node.subnode().fn(), typeRestrictions);
  case opWithMeta:
    return codegen(node, node.subnode().withmeta(), typeRestrictions);
  default: {
    auto env = node.env();
    throwCodeGenerationException(
        std::string("Compiler does not support the following op yet: ") +
            Op_Name(node.op()),
        node.form(), env.file(), env.line(), env.column());
  }
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
  case opFn:
    return getType(node, node.subnode().fn(), typeRestrictions);
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
  case opIsInstance:
    return getType(node, node.subnode().isinstance(), typeRestrictions);
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
  case opNew:
    return getType(node, node.subnode().new_(), typeRestrictions);
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
  case opThrow:
    return this->getType(
        node,
        static_cast<const clojure::rt::protobuf::bytecode::ThrowNode &>(
            node.subnode().throw_()),
        typeRestrictions);
  case opTry:
  //   return getType(node, node.subnode().try_(), typeRestrictions);
  case opVar:
    return getType(node, node.subnode().var(), typeRestrictions);
  case opWithMeta:
    return getType(node, node.subnode().withmeta(), typeRestrictions);
  default: {
    auto env = node.env();
    throwCodeGenerationException(
        std::string("Compiler does not support the following op yet: ") +
            Op_Name(node.op()),
        node.form(), env.file(), env.line(), env.column());
  }
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

  case opThrow:
  case opInvoke:
    return true;
  default:
    return true;
  }
}

} // namespace rt
