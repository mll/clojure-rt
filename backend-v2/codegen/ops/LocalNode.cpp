#include "../CodeGen.h"

using namespace std;
using namespace llvm;
using namespace clojure::rt::protobuf::bytecode;

namespace rt {

TypedValue CodeGen::codegen(const Node &node, const LocalNode &subnode,
                            const ObjectTypeSet &typeRestrictions) {
  switch (subnode.local()) {
  case localTypeArg:
  case localTypeLet:
  case localTypeLoop:
  case localTypeCatch: {
    auto name = subnode.name();
    auto *val = variableBindingStack.find(name);
    if (!val) {
      throwCodeGenerationException(string("Unknown variable: ") + name, node);
    }
    return *val;
  }
  default:
    throwCodeGenerationException(
        string("Compiler does not fully support the following local type yet: ") +
            to_string(subnode.local()),
        node);
  }
}

ObjectTypeSet CodeGen::getType(const Node &node, const LocalNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
  switch (subnode.local()) {
  case localTypeArg:
  case localTypeLet:
  case localTypeLoop:
  case localTypeCatch: {
    auto name = subnode.name();
    auto *type = variableTypesBindingsStack.find(name);
    if (!type) {
      throwCodeGenerationException(string("Unknown variable: ") + name, node);
    }
    return *type;
  }
  default:
    throwCodeGenerationException(
        string("Compiler does not fully support the following local type yet: ") +
            to_string(subnode.local()),
        node);
  }
}

} // namespace rt
