#include "../../tools/EdnParser.h"
#include "../../tools/RTValueWrapper.h"
#include "../../types/ConstantBool.h"
#include "../../types/ConstantDouble.h"
#include "../../types/ConstantInteger.h"
#include "../CodeGen.h"
#include "bridge/Exceptions.h"
#include "bytecode.pb.h"
#include "codegen/TypedValue.h"
#include "types/ObjectTypeSet.h"
#include <sstream>

using namespace std;
using namespace llvm;
using namespace clojure::rt::protobuf::bytecode;

namespace rt {

TypedValue CodeGen::codegen(const Node &node, const HostInteropNode &subnode,
                            const ObjectTypeSet &typeRestrictions) {
  CleanupChainGuard guard(*this);

  // 1. Codegen target
  auto targetVal = codegen(subnode.target(), ObjectTypeSet::all());
  guard.push(targetVal);

  // 2. Host interop (getter/0-arg call) uses the same mechanism as instance
  // call
  return this->invokeManager.generateInstanceCall(subnode.morf(), targetVal, {},
                                                  &guard, &node);
}

ObjectTypeSet CodeGen::getType(const Node &node, const HostInteropNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
  /* Instance call is always dynamic because protocols can change at any time so
   * we always need IC. Once we implement T2 we will add the static path again
   */
  return ObjectTypeSet::dynamicType();
  // if (subnode.isassignable()) {
  //   return ObjectTypeSet::all();
  // }

  // auto targetType = getType(subnode.target(), ObjectTypeSet::all());
  // return this->invokeManager.predictInstanceCallType(subnode.morf(),
  // targetType,
  //                                                    {});
}

} // namespace rt