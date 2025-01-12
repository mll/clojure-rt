#include "../codegen.h"  
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <sstream>
#include "../cljassert.h"

using namespace std;
using namespace llvm;

extern "C" {
  #include "../runtime/Keyword.h"
}


