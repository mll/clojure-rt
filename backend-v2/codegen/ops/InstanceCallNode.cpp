#include "../../tools/EdnParser.h"
#include "../../tools/RTValueWrapper.h"
#include "../../types/ConstantBool.h"
#include "../../types/ConstantDouble.h"
#include "../../types/ConstantInteger.h"
#include "../CodeGen.h"
#include "bridge/Exceptions.h"
#include "bytecode.pb.h"
#include "codegen/TypedValue.h"
#include <sstream>

using namespace std;
using namespace llvm;
using namespace clojure::rt::protobuf::bytecode;

namespace rt {

TypedValue CodeGen::codegen(const Node &node, const InstanceCallNode &subnode,
                             const ObjectTypeSet &typeRestrictions) {
  CleanupChainGuard guard(*this);

  // 1. Codegen instance
  auto instanceVal = codegen(subnode.instance(), ObjectTypeSet::all());
  guard.push(instanceVal);

  // 2. Codegen arguments
  std::vector<TypedValue> args;
  std::vector<ObjectTypeSet> argTypes;
  bool allDetermined = instanceVal.type.isDetermined();
  
  for (int i = 0; i < subnode.args_size(); i++) {
    auto t = codegen(subnode.args(i), ObjectTypeSet::all());
    if (!t.type.isDetermined())
      allDetermined = false;
    argTypes.push_back(t.type.unboxed());
    args.push_back(t);
    guard.push(t);
  }

  // 3. Resolve Class
  if (!instanceVal.type.isDetermined()) {
    throwCodeGenerationException("Dynamic instance call not implemented yet", node);
  }

  auto objType = instanceVal.type.determinedType();
  
  ::Class *targetClass = this->compilerState.classRegistry.getCurrent((int32_t)objType);

  if (objType == classType && instanceVal.type.getConstant()) {
    if (auto *cc = dynamic_cast<ConstantClass *>(instanceVal.type.getConstant())) {
      ::Class *constantClassPtr = (::Class *)cc->value;
      if (targetClass && targetClass != constantClassPtr) {
        // The constant class is more specific than the generic Class hull
        Ptr_release(targetClass);
        targetClass = constantClassPtr;
        Ptr_retain(targetClass);
      } else if (!targetClass) {
        targetClass = constantClassPtr;
        Ptr_retain(targetClass);
      }
    }
  }

  if (!targetClass) {
    string name = ObjectTypeSet::toHumanReadableName(objType);
    targetClass = this->compilerState.classRegistry.getCurrent(name.c_str());
  }

  PtrWrapper<Class> cls(targetClass);
  if (!cls) {
    std::ostringstream oss;
    oss << "Class not found for instance type: " << ObjectTypeSet::toHumanReadableName(objType);
    throwCodeGenerationException(oss.str(), node);
  }

  auto *ext = static_cast<ClassDescription *>(cls->compilerExtension);
  if (!ext) {
    std::ostringstream oss;
    oss << "Class " << ObjectTypeSet::toHumanReadableName(objType) << " does not have compiler metadata";
    throwCodeGenerationException(oss.str(), node);
  }

  auto m = subnode.method();
  auto it_method = ext->instanceFns.find(m);
  if (it_method == ext->instanceFns.end()) {
    std::ostringstream oss;
    oss << "Class " << ObjectTypeSet::toHumanReadableName(objType) << " does not have an instance method " << m;
    throwCodeGenerationException(oss.str(), node);
  }

  const std::vector<IntrinsicDescription> &versions = it_method->second;
  const IntrinsicDescription *bestMatch = nullptr;

  if (allDetermined) {
    // 1. Try exact match
    for (const auto &id : versions) {
      // Instance method intrinsics expect 'this' as the first argument in their argTypes
      if (id.argTypes.size() == args.size() + 1) {
        bool match = true;
        // Check 'this' type
        if (instanceVal.type.restriction(id.argTypes[0]).isEmpty()) {
          match = false;
        } else {
          for (size_t i = 0; i < args.size(); i++) {
            if (argTypes[i].restriction(id.argTypes[i + 1]).isEmpty()) {
              match = false;
              break;
            }
          }
        }
        if (match) {
          bestMatch = &id;
          break;
        }
      }
    }
  }

  if (bestMatch) {
    std::vector<TypedValue> unboxedArgs;
    // Push 'this'
    unboxedArgs.push_back(this->valueEncoder.unbox(instanceVal));
    // Push args
    for (size_t i = 0; i < args.size(); i++) {
      unboxedArgs.push_back(this->valueEncoder.unbox(args[i]));
    }
    
    // Invoke intrinsic. CleanupChainGuard ensures resources are released.
    // "functions consume" convention: generateIntrinsic takes guard and will pop/release.
    auto result =
        this->invokeManager.generateIntrinsic(*bestMatch, unboxedArgs, &guard);

    return result;
  }

  if (allDetermined) {
    std::ostringstream oss;
    oss << "No matching overload found for " << ObjectTypeSet::toHumanReadableName(objType) << "." << m
        << " with arguments: [";
    for (size_t i = 0; i < argTypes.size(); i++) {
      oss << argTypes[i].toString() << (i == argTypes.size() - 1 ? "" : ", ");
    }
    oss << "]";
    throwCodeGenerationException(oss.str(), node);
  }

  throwCodeGenerationException("Dynamic dispatch for InstanceCallNode not implemented yet", node);
  return TypedValue(ObjectTypeSet::all(), nullptr);
}

ObjectTypeSet CodeGen::getType(const Node &node,
                               const InstanceCallNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
  auto instanceType = getType(subnode.instance(), ObjectTypeSet::all());
  std::vector<ObjectTypeSet> argTypes;
  bool allDetermined = instanceType.isDetermined();
  
  for (int i = 0; i < subnode.args_size(); i++) {
    auto t = getType(subnode.args(i), ObjectTypeSet::all());
    if (!t.isDetermined())
      allDetermined = false;
    argTypes.push_back(t.unboxed());
  }

  if (!instanceType.isDetermined())
    return ObjectTypeSet::dynamicType();

  auto objType = instanceType.determinedType();
  ::Class *targetClass = compilerState.classRegistry.getCurrent((int32_t)objType);

  if (objType == classType && instanceType.getConstant()) {
    if (auto *cc = dynamic_cast<ConstantClass *>(instanceType.getConstant())) {
      ::Class *constantClassPtr = (::Class *)cc->value;
      if (targetClass && targetClass != constantClassPtr) {
        Ptr_release(targetClass);
        targetClass = constantClassPtr;
        Ptr_retain(targetClass);
      } else if (!targetClass) {
        targetClass = constantClassPtr;
        Ptr_retain(targetClass);
      }
    }
  }

  if (!targetClass) {
    string name = ObjectTypeSet::toHumanReadableName(objType);
    targetClass = compilerState.classRegistry.getCurrent(name.c_str());
  }

  PtrWrapper<Class> cls(targetClass);
  if (!cls)
    return ObjectTypeSet::dynamicType();

  auto *ext = static_cast<ClassDescription *>(cls->compilerExtension);
  if (!ext)
    return ObjectTypeSet::dynamicType();

  auto it_method = ext->instanceFns.find(subnode.method());
  if (it_method == ext->instanceFns.end())
    return ObjectTypeSet::dynamicType();

  const std::vector<IntrinsicDescription> &versions = it_method->second;

  if (allDetermined) {
    for (const auto &id : versions) {
      if (id.argTypes.size() == argTypes.size() + 1) {
        bool match = true;
        if (instanceType.restriction(id.argTypes[0]).isEmpty()) {
          match = false;
        } else {
          for (size_t i = 0; i < argTypes.size(); i++) {
            if (argTypes[i].restriction(id.argTypes[i + 1]).isEmpty()) {
              match = false;
              break;
            }
          }
        }
        if (match) {
          // Fold instance call if possible
          std::vector<ObjectTypeSet> allArgs = {instanceType};
          allArgs.insert(allArgs.end(), argTypes.begin(), argTypes.end());
          ObjectTypeSet folded = invokeManager.foldIntrinsic(id, allArgs);
          if (folded.isDetermined() && folded.getConstant()) {
            return folded;
          }
          return id.returnType;
        }
      }
    }
  }

  return ObjectTypeSet::dynamicType();
}

} // namespace rt