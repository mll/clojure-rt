#ifndef RT_CLOSEDOVERS
#define RT_CLOSEDOVERS
#include "../codegen.h"  
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <sstream>
#include "../cljassert.h"


extern "C" {
  #include "../runtime/Keyword.h"
}

void addClosedOversToFunctionJIT(struct FunctionMethod *method, FunctionJIT &jitInfo);

#endif
