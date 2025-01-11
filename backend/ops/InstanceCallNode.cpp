#include "../codegen.h"
#include <vector>
#include <algorithm>

using namespace std;
using namespace llvm;

TypedValue CodeGenerator::codegen(const Node &node, const InstanceCallNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto type = getType(node, typeRestrictions);
  auto target = codegen(subnode.instance(), ObjectTypeSet::all());
  auto targetType = target.first;
  auto ptrT = Type::getInt8Ty(*TheContext)->getPointerTo();
  std::vector<TypedValue> args;
  for (auto arg: subnode.args()) args.push_back(codegen(arg, ObjectTypeSet::all())); // evaluate all arguments even if instance call resolution fails
  auto methodName = subnode.method();
  uint64_t classId = 0;
  
  if (targetType.isDetermined()) {
    if (targetType.determinedType() != deftypeType) {
      classId = targetType.determinedType();
    } else if (targetType.isDeterminedClass()) {
      classId = targetType.determinedClass();
    }
  }
  
  if (classId) { // Static dispatch
    std::vector<uint64_t> candidateClasses {classId};
    if (targetType.isDeterminedClass()) candidateClasses.push_back(deftypeType);
    
    // Static method
    for (uint64_t classId: candidateClasses) {
      auto classMethods = TheProgramme->InstanceCallLibrary.find(classId);
      if (classMethods == TheProgramme->InstanceCallLibrary.end()) throw CodeGenerationException(string("Class ") + to_string(classId) + string(" not found"), node); // class not found
      
      auto methods = classMethods->second.find(methodName);
      if (methods != classMethods->second.end()) { // class and method found
        vector<ObjectTypeSet> types {targetType};
        vector<TypedValue> methodArgs {target};
        for (auto arg: args) types.push_back(arg.first);
        methodArgs.insert(methodArgs.end(), args.begin(), args.end());
        string requiredTypes = ObjectTypeSet::typeStringForArgs(types);
        for (auto method: methods->second) {
          auto methodTypes = typesForArgString(node, method.first);
          if(method.first != requiredTypes) continue; 
          return method.second.second(this, methodName + " " + requiredTypes, node, methodArgs);
        }
      }
    }
  }
  
  Function *parentFunction = Builder->GetInsertBlock()->getParent();
  Value *targetRuntimeType = getRuntimeObjectType(target.second);
  BasicBlock *failedBB = llvm::BasicBlock::Create(*TheContext, "instance_call_failed", parentFunction);
  BasicBlock *finalBB = llvm::BasicBlock::Create(*TheContext, "instance_call_final", parentFunction);
  Value *methodNameValue = dynamicString(methodName.c_str()); // CONSIDER: Pass std::string instead of building runtime String and then immediately discarding it?
  dynamicRetain(methodNameValue);
  SwitchInst *cond = Builder->CreateSwitch(targetRuntimeType, failedBB, targetType.size());
  Builder->SetInsertPoint(failedBB);
  runtimeException(CodeGenerationException(string("Unexpected type!"), node));
  Builder->CreateUnreachable();
  Builder->SetInsertPoint(finalBB);
  PHINode *phiNode = Builder->CreatePHI(ptrT, targetType.size() + targetType.classesSize(), "instance_call_phi");
  
  for (auto it = targetType.internalBegin(); it != targetType.internalEnd(); ++it) {
    objectType t = *it;
    BasicBlock *inTypeBB = llvm::BasicBlock::Create(*TheContext, "objectType_" + to_string(t), parentFunction);
    cond->addCase(ConstantInt::get(*TheContext, APInt(32, t, false)), inTypeBB);
    Builder->SetInsertPoint(inTypeBB);
    
    // In case of deftype, check class fields and methods first
    if (t == deftypeType) {
      // TODO after class methods implemented
    }
    
    // Primitive method
    Value *statePtr = Builder->CreateBitOrPointerCast(ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, (uint64_t) &*TheProgramme, false)), ptrT);
//    Value *nodePtr = Builder->CreateBitOrPointerCast(ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, (uint64_t) &node, false)), ptrT);
    Value *typeValue = ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, t, false));
    std::vector<Type *> primitiveMethodTypes {ptrT, ptrT, Type::getInt64Ty(*TheContext), Type::getInt64Ty(*TheContext)};
    for (unsigned long i = 0; i < args.size(); ++i) primitiveMethodTypes.push_back(Type::getInt64Ty(*TheContext));
    Value *extraArgsCount = ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, args.size(), false));
    std::vector<Value *> primitiveMethodValues {statePtr, methodNameValue, typeValue, extraArgsCount};
    for (auto arg: args) {
      Value *argTypeValue = arg.first.isDetermined()
        ? ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, arg.first.determinedType(), false))
        : getRuntimeObjectType(arg.second);
      primitiveMethodValues.push_back(argTypeValue);
    }
    Value *methodValue = callRuntimeFun("getPrimitiveMethod", ptrT, primitiveMethodTypes, primitiveMethodValues);
    Value *methodNotFoundValue = Builder->CreateICmpEQ(methodValue, Constant::getNullValue(ptrT));
    BasicBlock *methodFound = BasicBlock::Create(*TheContext, "method_found", parentFunction);
    BasicBlock *methodMissing = BasicBlock::Create(*TheContext, "method_missing", parentFunction);
    Builder->CreateCondBr(methodNotFoundValue, methodMissing, methodFound);
    Builder->SetInsertPoint(methodFound);
    FunctionType *FT = FunctionType::get(ptrT, {ptrT}, false);
    Value *callablePointer = Builder->CreatePointerCast(methodValue, FT->getPointerTo());
    Value *methodCalledValue = Builder->CreateCall(FunctionCallee(FT, callablePointer), {target.second}, "method_call");
    phiNode->addIncoming(methodCalledValue, methodFound);
    Builder->CreateBr(finalBB);
    
    Builder->SetInsertPoint(methodMissing);
    runtimeException(CodeGenerationException(string("Method ") + methodName + string(" of class ") + to_string(t) + string(" not found"), node));
    Builder->CreateUnreachable();
  }
  Builder->SetInsertPoint(finalBB);
  return TypedValue(ObjectTypeSet::all(), phiNode);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const InstanceCallNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto targetType = getType(subnode.instance(), ObjectTypeSet::all());
  auto methodName = subnode.method();
  uint64_t classId = 0;
  
  if (targetType.isDetermined()) {
    if (targetType.determinedType() != deftypeType) {
      classId = targetType.determinedType();
    } else if (targetType.isDeterminedClass()) {
      classId = targetType.determinedClass();
    }
  }
  
  if (classId) { // Static dispatch
    auto classMethods = TheProgramme->InstanceCallLibrary.find(classId);
    if (classMethods == TheProgramme->InstanceCallLibrary.end()) return ObjectTypeSet::dynamicType(); // class not found
    
    auto methods = classMethods->second.find(methodName);
    if (methods != classMethods->second.end()) { // class and method found
      vector<ObjectTypeSet> types {targetType};
      for (auto arg: subnode.args()) types.push_back(getType(arg, ObjectTypeSet::all()));
      string requiredTypes = ObjectTypeSet::typeStringForArgs(types);
      for (auto method: methods->second) {
        auto methodTypes = typesForArgString(node, method.first);
        if(method.first != requiredTypes) continue;
        return method.second.first(this, methodName + " " + requiredTypes, node, types).restriction(typeRestrictions);
      }
      return ObjectTypeSet::dynamicType(); // class found, static method found, but does not expect arity 0
    } else { // class found, static method not found - dynamic method or not present at all
      return ObjectTypeSet::dynamicType();
    }
  }
  
  // CONSIDER: Check if dynamic method exists (this tells us nothing about its type, but lets us throw an exception)
  return ObjectTypeSet::all();
}
