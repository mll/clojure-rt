#include "../codegen.h"  

using namespace std;
using namespace llvm;

extern "C" {
  void release(void *obj);
  void retain(void *obj);
} 

TypedValue CodeGenerator::codegen(const Node &node, const InstanceFieldNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto type = getType(node, typeRestrictions);
  auto target = codegen(subnode.instance(), ObjectTypeSet::all());
  auto targetType = target.first;
  auto ptrT = Type::getInt8Ty(*TheContext)->getPointerTo();
  auto fieldName = subnode.field();
  uint64_t classId = 0;
  
  if (targetType.isDetermined()) {
    if (targetType.determinedType() != deftypeType) {
      classId = targetType.determinedType();
    } else if (targetType.isDeterminedClass()) {
      classId = targetType.determinedClass();
    }
  }
  
  if (classId) { // Static dispatch
    if (targetType.determinedType() == deftypeType) {
      auto classIt = TheProgramme->DefinedClasses.find(classId);
      if (classIt == TheProgramme->DefinedClasses.end()) throw CodeGenerationException(string("Class ") + to_string(classId) + string(" not found"), node); // class not found
      Class *_class = classIt->second;
      retain(_class);
      int64_t fieldIndex = Class_fieldIndex(_class, String_createDynamicStr(fieldName.c_str()));
      if (fieldIndex == -1) {
        throw CodeGenerationException(string("Field ") + fieldName + string(" of class ") + to_string(classId) + string(" not found"), node); // class not found
      }
      Value *fieldIndexValue = ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, fieldIndex, true));
      Value *fieldValue = callRuntimeFun("Deftype_getIndexedField", ptrT, {ptrT, Type::getInt64Ty(*TheContext)}, {target.second, fieldIndexValue});
      
      return TypedValue(ObjectTypeSet::all(), fieldValue);
    } else {
      // TODO: Primitive types
      throw CodeGenerationException(string("Field access to primitive type ") + to_string(classId) + string(" not implemented"), node); // class not found
    }
  }
  
  Function *parentFunction = Builder->GetInsertBlock()->getParent();
  Value *targetRuntimeType = getRuntimeObjectType(target.second);
  BasicBlock *failedBB = llvm::BasicBlock::Create(*TheContext, "instance_field_failed", parentFunction);
  BasicBlock *finalBB = llvm::BasicBlock::Create(*TheContext, "instance_field_final", parentFunction);
  Value *fieldNameValue = dynamicString(fieldName.c_str()); // CONSIDER: Pass std::string instead of building runtime String and then immediately discarding it?
  dynamicRetain(fieldNameValue);
  SwitchInst *cond = Builder->CreateSwitch(targetRuntimeType, failedBB, targetType.size());
  Builder->SetInsertPoint(failedBB);
  runtimeException(CodeGenerationException(string("Unexpected type!"), node));
  Builder->CreateUnreachable();
  Builder->SetInsertPoint(finalBB);
  PHINode *phiNode = Builder->CreatePHI(ptrT, targetType.size() + targetType.classesSize(), "host_interop_phi");
  
  for (auto it = targetType.internalBegin(); it != targetType.internalEnd(); ++it) {
    objectType t = *it;
    BasicBlock *typeBB = llvm::BasicBlock::Create(*TheContext, "objectType_" + to_string(t), parentFunction);
    cond->addCase(ConstantInt::get(*TheContext, APInt(32, t, false)), typeBB);
    Builder->SetInsertPoint(typeBB);
    if (t == deftypeType) {
      dynamicRetain(target.second);
      Value *classRuntimeValue = callRuntimeFun("Deftype_getClass", ptrT, {ptrT}, {target.second});
      if (targetType.anyClass()) {
        Value *fieldIndex = callRuntimeFun("Class_fieldIndex", Type::getInt64Ty(*TheContext), {ptrT, ptrT}, {classRuntimeValue, fieldNameValue});
        Value *indexValidator = Builder->CreateICmpEQ(fieldIndex, ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, -1, true)));
        BasicBlock *classFieldFoundBB = llvm::BasicBlock::Create(*TheContext, "dynamic_class_field_lookup", parentFunction);
        BasicBlock *classFieldFailedBB = llvm::BasicBlock::Create(*TheContext, "dynamic_class_field_lookup_failed", parentFunction);
        Builder->CreateCondBr(indexValidator, classFieldFailedBB, classFieldFoundBB);
        Builder->SetInsertPoint(classFieldFailedBB);
        runtimeException(CodeGenerationException("Field " + fieldName + " not found!", node));
        Builder->CreateUnreachable();
        Builder->SetInsertPoint(classFieldFoundBB);
        Value *fieldValue = callRuntimeFun("Deftype_getIndexedField", ptrT, {ptrT, Type::getInt64Ty(*TheContext)}, {target.second, fieldIndex});
        phiNode->addIncoming(fieldValue, classFieldFoundBB);
        Builder->CreateBr(finalBB);
      } else {
        Value *classIdPtr = Builder->CreateStructGEP(runtimeClassType(), classRuntimeValue, 0, "get_class_id");
        Value *classIdValue = Builder->CreateLoad(Type::getInt64Ty(*TheContext), classIdPtr, "class_id");
        BasicBlock *failedClassSwitchBB = llvm::BasicBlock::Create(*TheContext, "host_interop_class_switch_failed", parentFunction);
        SwitchInst *classCond = Builder->CreateSwitch(classRuntimeValue, failedBB, targetType.classesSize());
        for (auto it = targetType.internalClassesBegin(); it != targetType.internalClassesEnd(); ++it) {
          classId = *it;
          BasicBlock *classBB = llvm::BasicBlock::Create(*TheContext, "class_" + to_string(classId), parentFunction);
          classCond->addCase(ConstantInt::get(*TheContext, APInt(64, classId, false)), classBB);
          Builder->SetInsertPoint(classBB);
          auto classIt = TheProgramme->DefinedClasses.find(classId);
          if (classIt == TheProgramme->DefinedClasses.end()) {
            runtimeException(CodeGenerationException("Class " + to_string(classId) + " not found", node));
            Builder->CreateUnreachable();
          } else {
            retain(classIt->second);
            int64_t fieldIndex = Class_fieldIndex(classIt->second, String_createDynamicStr(fieldName.c_str()));
            if (fieldIndex == -1) {
              runtimeException(CodeGenerationException("Field " + fieldName + " in class " + to_string(classId) + " not found!", node));
              Builder->CreateUnreachable();
            } else {
              Value *fieldIndexValue = ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, fieldIndex, true));
              Value *fieldValue = callRuntimeFun("Deftype_getIndexedField", ptrT, {ptrT, Type::getInt64Ty(*TheContext)}, {target.second, fieldIndexValue});
              phiNode->addIncoming(fieldValue, classBB);
              Builder->CreateBr(finalBB);
            }
          }
        }
      }
    } else {
      Value *statePtr = Builder->CreateBitOrPointerCast(ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, (uint64_t) &*TheProgramme, false)), ptrT);
      Value *typeValue = ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, t, false));
      Value *fieldValue = callRuntimeFun("getPrimitiveField", ptrT, {ptrT, Type::getInt64Ty(*TheContext), ptrT}, {statePtr, typeValue, fieldNameValue});
      Value *fieldNotFoundValue = Builder->CreateICmpEQ(fieldValue, Constant::getNullValue(ptrT));
      BasicBlock *fieldMissing = BasicBlock::Create(*TheContext, "field_missing", parentFunction);
      Builder->CreateCondBr(fieldNotFoundValue, fieldMissing, finalBB);
      phiNode->addIncoming(fieldValue, typeBB);
      
      Builder->SetInsertPoint(fieldMissing);
      runtimeException(CodeGenerationException(string("Field ") + fieldName + string(" of class ") + to_string(t) + string(" not found"), node));
      Builder->CreateUnreachable();
    }
  }
  Builder->SetInsertPoint(finalBB);
  return TypedValue(ObjectTypeSet::all(), phiNode);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const InstanceFieldNode &subnode, const ObjectTypeSet &typeRestrictions) {
  // Field can be any type
  return ObjectTypeSet::all();
}
