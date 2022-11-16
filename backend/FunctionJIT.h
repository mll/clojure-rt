#ifndef RT_FUNCTION_JIT
#define RT_FUNCTION_JIT

#include "ObjectTypeSet.h"
#include "protobuf/bytecode.pb.h"
#include <string>

using namespace clojure::rt::protobuf::bytecode;

struct FunctionJIT {
  FnMethodNode method;
  std::vector<ObjectTypeSet> args;
  ObjectTypeSet retVal;
  std::string name;
  /* Alternative way to select method */
  int64_t uniqueId = 0;
  int64_t methodIndex;
};

#endif
