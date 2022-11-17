#include "jit/jit.h"
#include "codegen.h"
#include "FunctionJIT.h"

extern "C" {
#include "runtime/Function.h"
#include <execinfo.h>
  
  void logExceptionD(const char *description) {
    void *array[1000];
    char **strings;
    int size, i;
    
    printf("%s\n", description);
    
    size = backtrace (array, 1000);
    strings = backtrace_symbols (array, size);
    if (strings != NULL)
      {
        
        printf ("Obtained %d stack frames:\n", size);
        for (i = 0; i < size; i++)
          printf ("%s\n", strings[i]);
      }
    
    free (strings);
    exit(1);
  }
  

  void printInt(int64_t i) {
    std::cout << "---> Int value: " << i << std::endl;
  }

  void printChar(char i) {
    std::cout << "---> Char value: '" << (int64_t)i << "'" << std::endl;
  }


  void *specialiseDynamicFn(void *jitPtr, void *funPtr, uint64_t retValType, uint64_t argCount, uint64_t argSignature, uint64_t packedArg) {    
    ClojureJIT *jit = (ClojureJIT *)jitPtr;
    struct Function *fun = (struct Function *)funPtr;
    struct FunctionMethod *method = NULL;
    for(int i=0; i<fun->methodCount; i++) {
      struct FunctionMethod *candidate = &(fun->methods[i]);
      if(argCount == candidate->fixedArity || (argCount > candidate->fixedArity && method->isVariadic)) {
        method = candidate;
        break;
      }
    } 
    if(method == NULL) {
      std::string name = "fn_" + std::to_string(fun->uniqueId); 
      std::string error = (std::string("Function: ") + name + " called with wrong number of arguments: " + std::to_string(argCount));
      logExceptionD(error.c_str());
      return NULL;
    }
   
    struct InvokationCache *entry = NULL;
    for(int i=0; i<INVOKATION_CACHE_SIZE; i++) { 
      /* Fast cache access */
      entry = &(method->invokations[i]);
      if(entry->signature == argSignature && entry->packed == packedArg && entry->returnType == retValType) return entry->fptr;
      if(entry->signature != 0) entry = NULL;
    } 

    std::vector<ObjectTypeSet> argT;
    ObjectTypeSet retValT;
    uint64_t mask = 0xff;

    for(int i=0; i < argCount; i++) {
      uint64_t type = (argSignature >> (8 * i)) & mask;
      unsigned char isBoxed = (packedArg >> i) & 1;
      argT.insert(argT.begin(), ObjectTypeSet((objectType)type, isBoxed));
    }

    retValT = retValType == 0 ? ObjectTypeSet::all() : ObjectTypeSet((objectType)retValType);

    std::string name = "fn_" + std::to_string(fun->uniqueId);   
    auto f = std::make_unique<FunctionJIT>();
    std::string rqName = CodeGenerator::fullyQualifiedMethodKey(name, argT, retValT);        
    f->args = argT;
    /* TODO - Return values should be probably handled differently. e.g. if the new function returns something boxed, 
       we should inspect it and decide if we want to use it or not for var inbvokations */ 
    f->retVal = retValT;
    f->uniqueId = fun->uniqueId;
    f->methodIndex = method->index;
    f->name = rqName;
    llvm::ExitOnError eo = llvm::ExitOnError();

    /* TODO - we probably need to modify TheProgramme here somehow, this needs more thinking */
    eo(jit->addAST(std::move(f)));

    auto ExprSymbol = eo(jit->lookup(rqName));
    void *retVal = (void *)ExprSymbol.getAddress();
      
    if(entry) { /* Store in cache if free entries available */
      entry->signature = argSignature;
      entry->packed = packedArg;
      entry->returnType = retValType;
      entry->fptr = retVal;
    }

    return retVal;
  }
}
