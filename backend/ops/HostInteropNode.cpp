#include "../codegen.h"

using namespace std;
using namespace llvm;

extern "C" {
  void release(void *obj);
  void retain(void *obj);
} 

TypedValue CodeGenerator::codegen(const Node &node, const HostInteropNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto type = getType(node, typeRestrictions);
  auto target = codegen(subnode.target(), ObjectTypeSet::all());
  auto targetType = target.first;
  auto ptrT = Type::getInt8Ty(*TheContext)->getPointerTo();
  auto methodOrFieldName = subnode.morf();
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
      
      auto methods = classMethods->second.find(methodOrFieldName);
      if (methods != classMethods->second.end()) { // class and method found
        vector<ObjectTypeSet> types {targetType};
        string requiredTypes = ObjectTypeSet::typeStringForArgs(types);
        for (auto method: methods->second) {
          auto methodTypes = typesForArgString(node, method.first);
          if(method.first != requiredTypes) continue; 
          return method.second.second(this, methodOrFieldName + " " + requiredTypes, node, {target});
        }
      }
    }
    // class found, method not found or found, but does not expect arity 0 - field or not present at all
    
    if (targetType.determinedType() == deftypeType) { // Fields of class
      auto classIt = TheProgramme->DefinedClasses.find(classId);
      if (classIt == TheProgramme->DefinedClasses.end()) throw CodeGenerationException(string("Class ") + to_string(classId) + string(" not found"), node); // class not found
      Class *_class = classIt->second;
      retain(_class);
      int64_t fieldIndex = Class_fieldIndex(_class, Keyword_create(String_createDynamicStr(methodOrFieldName.c_str())));
      if (fieldIndex > -1) {
        Value *fieldIndexValue = ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, fieldIndex, true));
        Value *fieldValue = callRuntimeFun("Deftype_getIndexedField", ptrT, {ptrT, Type::getInt64Ty(*TheContext)}, {target.second, fieldIndexValue});
        return TypedValue(ObjectTypeSet::all(), fieldValue); 
      }
    }
    
    // Primitive types don't have fields, only methods
    throw CodeGenerationException(string("Field access to primitive type ") + to_string(classId) + string(" not implemented"), node); // class not found
  }
  
  // Reflection
  Function *parentFunction = Builder->GetInsertBlock()->getParent();
  Value *targetRuntimeType = getRuntimeObjectType(target.second);
  BasicBlock *failedBB = llvm::BasicBlock::Create(*TheContext, "host_interop_failed", parentFunction);
  BasicBlock *finalBB = llvm::BasicBlock::Create(*TheContext, "host_interop_final", parentFunction);
  SwitchInst *cond = Builder->CreateSwitch(targetRuntimeType, failedBB, targetType.size());
  Builder->SetInsertPoint(failedBB);
  runtimeException(CodeGenerationException(string("Unexpected type!"), node));
  Builder->CreateUnreachable();
  Builder->SetInsertPoint(finalBB);
  PHINode *phiNode = Builder->CreatePHI(ptrT, targetType.size() + targetType.classesSize(), "host_interop_phi");
  
  for (auto it = targetType.internalBegin(); it != targetType.internalEnd(); ++it) {
    objectType t = *it;
    BasicBlock *inTypeBB = llvm::BasicBlock::Create(*TheContext, "objectType_" + to_string(t), parentFunction);
    BasicBlock *outTypeBB = (t == deftypeType) ? llvm::BasicBlock::Create(*TheContext, "objectType_deftype_only", parentFunction) : inTypeBB;
    cond->addCase(ConstantInt::get(*TheContext, APInt(32, t, false)), inTypeBB);
    Builder->SetInsertPoint(inTypeBB);
    
    // In case of deftype, check class fields and methods first
    if (t == deftypeType) {
      dynamicRetain(target.second);
      Value *classRuntimeValue = callRuntimeFun("Deftype_getClass", ptrT, {ptrT}, {target.second});
      if (targetType.anyClass()) { // Totally unknown class, can't switch
        Value *fieldNameAsKeyword = dynamicKeyword(methodOrFieldName.c_str());
        Value *fieldIndex = callRuntimeFun("Class_fieldIndex", Type::getInt64Ty(*TheContext), {ptrT, ptrT}, {classRuntimeValue, fieldNameAsKeyword});
        Value *indexValidator = Builder->CreateICmpEQ(fieldIndex, ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, -1, true)));
        BasicBlock *classFieldFoundBB = llvm::BasicBlock::Create(*TheContext, "dynamic_class_field_lookup_successful", parentFunction);
        BasicBlock *classMethodBB = llvm::BasicBlock::Create(*TheContext, "dynamic_class_field_lookup_failed", parentFunction);
        Builder->CreateCondBr(indexValidator, classMethodBB, classFieldFoundBB);
        Builder->SetInsertPoint(classFieldFoundBB);
        Value *fieldValue = callRuntimeFun("Deftype_getIndexedField", ptrT, {ptrT, Type::getInt64Ty(*TheContext)}, {target.second, fieldIndex});
        phiNode->addIncoming(fieldValue, classFieldFoundBB);
        Builder->CreateBr(finalBB);
        Builder->SetInsertPoint(classMethodBB);
        Keyword *methodNameKeyword = Keyword_create(String_createDynamicStr(methodOrFieldName.c_str()));
        vector<Type *> methodValueArgTypes {ptrT, ptrT, Type::getInt64Ty(*TheContext)};
        vector<Value *> methodValueArgs {classRuntimeValue, ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, (uint64_t) methodNameKeyword, true)), ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, 0, true))};
        Value *methodValue = callRuntimeFun("Class_resolveInstanceCall", ptrT, methodValueArgTypes, methodValueArgs);
        Value *methodNotFoundValue = Builder->CreateICmpEQ(methodValue, Constant::getNullValue(ptrT));
        BasicBlock *methodFoundBB = llvm::BasicBlock::Create(*TheContext, "dynamic_class_method_lookup_successful", parentFunction);
        Builder->CreateCondBr(methodNotFoundValue, outTypeBB, methodFoundBB);
        Builder->SetInsertPoint(methodFoundBB);
        // TODO
      } else { // One of N classes, switch on them
//        Alek - why do we have those variables here? Why aren't they used.
//        Value *classIdPtr = Builder->CreateStructGEP(runtimeClassType(), classRuntimeValue, 1, "get_class_id");
//        Value *classIdValue = Builder->CreateLoad(Type::getInt64Ty(*TheContext), classIdPtr, "class_id");
//        BasicBlock *failedClassSwitchBB = llvm::BasicBlock::Create(*TheContext, "host_interop_class_switch_failed", parentFunction);
        SwitchInst *classCond = Builder->CreateSwitch(classRuntimeValue, failedBB, targetType.classesSize());
        for (auto it = targetType.internalClassesBegin(); it != targetType.internalClassesEnd(); ++it) {
          classId = *it;
          BasicBlock *classBB = llvm::BasicBlock::Create(*TheContext, "class_" + to_string(classId), parentFunction);
          classCond->addCase(ConstantInt::get(*TheContext, APInt(64, classId, false)), classBB);
          Builder->SetInsertPoint(classBB);
          auto classIt = TheProgramme->DefinedClasses.find(classId);
          if (classIt == TheProgramme->DefinedClasses.end()) {
            Builder->CreateBr(outTypeBB);
          } else {
            retain(classIt->second);
            int64_t fieldIndex = Class_fieldIndex(classIt->second, Keyword_create(String_createDynamicStr(methodOrFieldName.c_str())));
            if (fieldIndex == -1) {
              Builder->CreateBr(outTypeBB);
            } else {
              Value *fieldIndexValue = ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, fieldIndex, true));
              Value *fieldValue = callRuntimeFun("Deftype_getIndexedField", ptrT, {ptrT, Type::getInt64Ty(*TheContext)}, {target.second, fieldIndexValue});
              phiNode->addIncoming(fieldValue, classBB);
              Builder->CreateBr(finalBB);
            }
          }
        }
      }
    }
    
    // Primitive method
    Builder->SetInsertPoint(outTypeBB);
    Value *statePtr = Builder->CreateBitOrPointerCast(ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, (uint64_t) &*TheProgramme, false)), ptrT);
//    Value *nodePtr = Builder->CreateBitOrPointerCast(ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, (uint64_t) &node, false)), ptrT);
    Value *typeValue = ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, t, false));
    Value *noExtraArgs = ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, 0, false));
    Value *methodOrFieldNameValue = dynamicString(methodOrFieldName.c_str());
    std::vector<Type *> primitiveMethodTypes {ptrT, ptrT, Type::getInt64Ty(*TheContext), Type::getInt64Ty(*TheContext)};
    std::vector<Value *> primitiveMethodValues {statePtr, methodOrFieldNameValue, typeValue, noExtraArgs};
    Value *methodValue = callRuntimeFun("getPrimitiveMethod", ptrT, primitiveMethodTypes, primitiveMethodValues, true);
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
    runtimeException(CodeGenerationException(string("Method or field ") + methodOrFieldName + string(" of class ") + to_string(t) + string(" not found"), node));
    Builder->CreateUnreachable();
  }
  Builder->SetInsertPoint(finalBB);
  return TypedValue(ObjectTypeSet::all(), phiNode);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const HostInteropNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto targetType = getType(subnode.target(), ObjectTypeSet::all());
  auto methodOrFieldName = subnode.morf();
  uint64_t classId = 0;
  
  // CONSIDER: methods (InstanceCallLibrary) vs fields
  
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
    
    for (uint64_t classId: candidateClasses) {
      auto classMethods = TheProgramme->InstanceCallLibrary.find(classId);
      if (classMethods == TheProgramme->InstanceCallLibrary.end()) return ObjectTypeSet::dynamicType(); // class not found
      
      auto methods = classMethods->second.find(methodOrFieldName);
      if (methods != classMethods->second.end()) { // class and method found
        vector<ObjectTypeSet> types {targetType};
        string requiredTypes = ObjectTypeSet::typeStringForArgs(types);
        for (auto method: methods->second) {
          auto methodTypes = typesForArgString(node, method.first);
          if(method.first != requiredTypes) continue;
          return method.second.first(this, methodOrFieldName + " " + requiredTypes, node, types).restriction(typeRestrictions);
        }
        return ObjectTypeSet::dynamicType(); // class found, static method found, but does not expect given arity
      } else { // class found, static method not found - field, dynamic method or not present at all
        return ObjectTypeSet::dynamicType();
      }
    }
  }

  return ObjectTypeSet::all();
}
