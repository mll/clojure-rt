#ifndef RT_FUNCTION_JIT
#define RT_FUNCTION_JIT

#include "ObjectTypeSet.h"
#include "protobuf/bytecode.pb.h"
#include <string>
using namespace clojure::rt::protobuf::bytecode;
using namespace std;

struct FunctionJIT {
  FnMethodNode method;
  vector<ObjectTypeSet> args;
  ObjectTypeSet retVal;
  string name;
};

#endif
