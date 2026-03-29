#include "../../tools/EdnParser.h"
#include "../../tools/RTValueWrapper.h"
#include "../../types/ConstantClass.h"
#include "../CodeGen.h"
#include "../invoke/InvokeManager.h"
#include "bridge/Exceptions.h"
#include "bytecode.pb.h"
#include "codegen/TypedValue.h"
#include "runtime/Class.h"
#include <sstream>

using namespace std;
using namespace llvm;
using namespace clojure::rt::protobuf::bytecode;

namespace rt {

TypedValue CodeGen::codegen(const Node &node, const NewNode &subnode,
                            const ObjectTypeSet &typeRestrictions) {
  CleanupChainGuard guard(*this);

  // 1. Get class name from type of class node
  auto classTypeSet = getType(subnode.class_(), ObjectTypeSet::all());

  auto *constVal = classTypeSet.getConstant();
  ConstantClass *classConst = dynamic_cast<ConstantClass *>(constVal);
  if (!classConst) {
    throwCodeGenerationException("Unable to resolve classname for new", node);
  }
  string className = classConst->value;
  if (className.find("class ") == 0) {
    className = className.substr(6);
  }

  // 2. Resolve class and its description
  ::Class *cls =
      this->compilerState.classRegistry.getCurrent(className.c_str());
  PtrWrapper<::Class> clsGuard(cls);
  if (!cls) {
    throwCodeGenerationException("Class " + className + " not found", node);
  }

  auto *ext = static_cast<ClassDescription *>(cls->compilerExtension);
  if (!ext) {
    throwCodeGenerationException(
        "Class " + className + " does not have compiler metadata", node);
  }

  // 3. Find matching constructor by arity
  const IntrinsicDescription *bestMatch = nullptr;
  for (const auto &ctor : ext->constructors) {
    if ((int32_t)ctor.argTypes.size() == subnode.args_size()) {
      bestMatch = &ctor;
      break;
    }
  }

  if (!bestMatch) {
    throwCodeGenerationException("Class " + className +
                                     " does not have a constructor of arity " +
                                     to_string(subnode.args_size()),
                                 node);
  }

  // 4. Codegen arguments
  std::vector<TypedValue> args;
  for (int i = 0; i < subnode.args_size(); i++) {
    auto t = codegen(subnode.args(i), ObjectTypeSet::all());
    args.push_back(t);
    guard.push(t);
  }

  std::vector<TypedValue> unboxedArgs;
  for (size_t i = 0; i < args.size(); i++) {
    if (!bestMatch->argTypes[i].isBoxedType()) {
      unboxedArgs.push_back(this->valueEncoder.unbox(args[i]));
    } else {
      unboxedArgs.push_back(this->valueEncoder.box(args[i]));
    }
  }

  return this->invokeManager.generateIntrinsic(*bestMatch, unboxedArgs, &guard);
}

ObjectTypeSet CodeGen::getType(const Node &node, const NewNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
  auto classTypeSet = getType(subnode.class_(), ObjectTypeSet::all());
  auto *constVal = classTypeSet.getConstant();
  ConstantClass *classConst = dynamic_cast<ConstantClass *>(constVal);
  if (!classConst) {
    return ObjectTypeSet::dynamicType();
  }

  string className = classConst->value;
  if (className.find("class ") == 0) {
    className = className.substr(6);
  }
  ScopedRef<::Class> cls(this->compilerState.classRegistry.getCurrent(className.c_str()));
  if (!cls)
    return ObjectTypeSet::dynamicType();

  return ObjectTypeSet((objectType)cls->registerId);
}

} // namespace rt
