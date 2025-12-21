#include "ClosedOvers.h"

using namespace std;
using namespace llvm;


void addClosedOversToFunctionJIT(struct FunctionMethod *method, FunctionJIT &jitInfo) {
    for(uint64_t i = 0; i < method->closedOversCount; i++) {
      auto type = CodeGenerator::typeOfObjectFromRuntime(method->closedOvers[i]);
      jitInfo.closedOvers.push_back(type);
    }
}


