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

  return TypedValue(ObjectTypeSet::all(), nullptr);
}

ObjectTypeSet CodeGen::getType(const Node &node,
                               const InstanceCallNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
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
    if (classMethods == TheProgramme->InstanceCallLibrary.end())
      return ObjectTypeSet::dynamicType(); // class not found

    auto methods = classMethods->second.find(methodName);
    if (methods != classMethods->second.end()) { // class and method found
      vector<ObjectTypeSet> types{targetType};
      for (auto arg : subnode.args())
        types.push_back(getType(arg, ObjectTypeSet::all()));
      string requiredTypes = ObjectTypeSet::typeStringForArgs(types);
      for (auto method : methods->second) {
        auto methodTypes = typesForArgString(node, method.first);
        if (method.first != requiredTypes)
          continue;
        return method.second
            .first(this, methodName + " " + requiredTypes, node, types)
            .restriction(typeRestrictions);
      }
      return ObjectTypeSet::dynamicType(); // class found, static method found,
                                           // but does not expect arity 0
    } else { // class found, static method not found - dynamic method or not
             // present at all
      return ObjectTypeSet::dynamicType();
    }
  }

  // CONSIDER: Check if dynamic method exists (this tells us nothing about its
  // type, but lets us throw an exception)
  return ObjectTypeSet::all();
}
