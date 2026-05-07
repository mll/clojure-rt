#include "../CodeGen.h"
#include "bridge/Exceptions.h"
#include "runtime/ObjectProto.h"

using namespace std;
using namespace llvm;

namespace rt {

TypedValue CodeGen::codegen(const Node &node, const ThrowNode &subnode,
                            const ObjectTypeSet &typeRestrictions) {
  TypedValue ex = codegen(subnode.exception(), ObjectTypeSet::all().boxed());
  TypedValue boxedEx = valueEncoder.box(ex);

  if (ex.type.isDetermined() && ex.type.determinedType() != exceptionType) {
    throwCodeGenerationException(
        "class " + ex.type.toString() +
            " cannot be cast to class java.lang.Throwable",
        node);
  }

  invokeManager.invokeRuntime("throwException_C", nullptr,
                              {ObjectTypeSet::all().boxed()}, {boxedEx});

  // This point is unreachable because throwException_C is [[noreturn]]
  return dynamicConstructor.createNil();
}

ObjectTypeSet CodeGen::getType(const Node &node, const ThrowNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
  return ObjectTypeSet(nilType);
}

} // namespace rt
