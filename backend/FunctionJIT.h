#ifndef RT_FUNCTION_JIT
#define RT_FUNCTION_JIT

#include "ObjectTypeSet.h"
#include "bytecode.pb.h"
#include <string>

using namespace clojure::rt::protobuf::bytecode;

struct FunctionJIT {
  std::vector<ObjectTypeSet> args;
  ObjectTypeSet retVal;
  int64_t uniqueId = 0;
  int64_t methodIndex;
  std::vector<ObjectTypeSet> closedOvers;
  std::string uniqueName() {
    auto retVall = "fn_" + std::to_string(uniqueId) + "_" + ObjectTypeSet::typeStringForArgs(args) + "_"+ ObjectTypeSet::typeStringForArg(retVal);
//    std::cout << "uniqueName: " << retVall << std::endl;
    return retVall;
  }
};

#endif
