#include "../../bridge/Exceptions.h"
#include "../../tools/EdnParser.h"
#include "../CodeGen.h"
#include <sstream>

using namespace std;
using namespace llvm;
using namespace clojure::rt::protobuf::bytecode;

namespace rt {

TypedValue CodeGen::codegen(const Node &node, const StaticCallNode &subnode,
                            const ObjectTypeSet &typeRestrictions) {
  std::vector<ObjectTypeSet> argTypes;
  std::vector<TypedValue> args;
  for (int i = 0; i < subnode.args_size(); i++) {
    auto t = codegen(subnode.args(i), ObjectTypeSet::all());
    argTypes.push_back(t.type.unboxed());
    args.push_back(t);
  }

  auto c = subnode.class_();
  auto m = subnode.method();
  string name = (c.rfind("class ", 0) == 0) ? c.substr(6) : c;

  Class *cls = compilerState.classRegistry.getCurrent(name.c_str());
  if (!cls) {
    std::ostringstream oss;
    oss << "Class not found: " << name;
    throwInternalInconsistencyException(oss.str());
  }

  auto *ext = static_cast<ClassDescription *>(cls->compilerExtension);
  if (!ext) {
    std::ostringstream oss;
    oss << "Class " << name << " does not have compiler metadata";
    throwInternalInconsistencyException(oss.str());
  }

  auto it_method = ext->staticFns.find(m);
  if (it_method == ext->staticFns.end()) {
    std::ostringstream oss;
    oss << "Class " << name << " does not have a static method " << m;
    throwInternalInconsistencyException(oss.str());
  }

  const std::vector<IntrinsicDescription> &versions = it_method->second;

  for (const auto &id : versions) {
    if (id.argTypes.size() == args.size()) {
      bool match = true;
      for (size_t i = 0; i < args.size(); i++) {
        if (!id.argTypes[i].contains(argTypes[i].determinedType())) {
          match = false;
          break;
        }
      }
      if (match) {
        return invokeManager.generateIntrinsic(id, args);
      }
    }
  }

  throwInternalInconsistencyException("No matching arity/types found for " +
                                      name + "/" + m);
}

ObjectTypeSet CodeGen::getType(const Node &node, const StaticCallNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
  std::vector<ObjectTypeSet> argTypes;
  for (int i = 0; i < subnode.args_size(); i++) {
    auto t = getType(subnode.args(i), ObjectTypeSet::all());
    argTypes.push_back(t.unboxed());
  }

  auto c = subnode.class_();
  auto m = subnode.method();
  string name = (c.rfind("class ", 0) == 0) ? c.substr(6) : c;

  Class *cls = compilerState.classRegistry.getCurrent(name.c_str());
  if (!cls)
    return ObjectTypeSet::dynamicType();

  auto *ext = static_cast<ClassDescription *>(cls->compilerExtension);
  if (!ext)
    return ObjectTypeSet::dynamicType();

  auto it_method = ext->staticFns.find(m);
  if (it_method == ext->staticFns.end())
    return ObjectTypeSet::dynamicType();

  const std::vector<IntrinsicDescription> &versions = it_method->second;

  for (const auto &id : versions) {
    if (id.argTypes.size() == argTypes.size()) {
      bool match = true;
      for (size_t i = 0; i < argTypes.size(); i++) {
        if (!id.argTypes[i].contains(argTypes[i].determinedType())) {
          match = false;
          break;
        }
      }
      if (match)
        return id.returnType;
    }
  }

  return ObjectTypeSet::dynamicType();
}

} // namespace rt
