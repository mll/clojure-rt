#include "ClosedOvers.h"
#include "../jit/jit.h"
#include "../codegen.h"
#include "../FunctionJIT.h"

extern "C" {
#include "../runtime/Function.h"
#include <execinfo.h>

/* Functions in this group represent some additional runtime functions that feed back directly to 
   JIT compiler. This is the basis of dynamic dispatch. They are never called directly. 
   Instead, compiled llvm code calls them when needed.
   

   We do not them in the actual runtime library because we want it to be small, 
   standalone and not link against LLVM (which is huge).
*/ 


  /* Logs exceptions that arise during runtime. It is temporary - the proper exception throwing 
     function will be introduced here once exception handling is done. */

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
  

/* These functions are used for debugging by CodeGenerator::printDebugValue */
  void printInt(int64_t i) {
    std::cout << "---> Int value: " << i << std::endl;
  }

  void printChar(char i) {
    std::cout << "---> Char value: '" << (int64_t)i << "'" << std::endl;
  }

  void printPointer(void *i) {
    std::cout << "---> Pointer value: '" << i << "'" << std::endl;
  }


/* Used in CodeGenerator::callDynamicFun to allow for runtime dynamic dispatch.
   The function receives argument types determined at runtime and therefore can 
   produce JIT body of a called function much more accurately and performantly than relying on 
   dynamic variables. */

  /* We pass argument types as 64 bit integers. Since type information is 8 bits, 
     this form of call can handle up to 24 arguments (three values). Original clojure can handle up to 20 arguments before going vararg, so this should be enough.
      */
 
  void *specialiseDynamicFn(void *jitPtr, 
                            void *funPtr, 
                            uint64_t retValType, 
                            uint64_t argCount, 
                            uint64_t argSignature1, 
                            uint64_t argSignature2, 
                            uint64_t argSignature3, 
                            uint64_t packedArg) {    
    ClojureJIT *jit = (ClojureJIT *)jitPtr;
    struct ClojureFunction *fun = (struct ClojureFunction *)funPtr;
    struct FunctionMethod *method = NULL;
    uint64_t argSignature[3] = { argSignature1, argSignature2, argSignature3 };
/* It will be cool to understand why do we need all the info inside the function object whereas 
   it could have been located somewhere inside TheProgramme inside the jit to which we 
   have a handle here... 
   
   Removing all the data from the function object could actually speed everything up a bit, 
   so maybe it is worth trying? The invokation cache certainly has its place here, but arities 
   could be sucked from JIT for sure */
    for(uint64_t i = 0; i < fun->methodCount; i++) {
      struct FunctionMethod *candidate = &(fun->methods[i]);
      if(argCount == candidate->fixedArity || (argCount > candidate->fixedArity && candidate->isVariadic)) {
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
   
    struct InvokationCache *entry = NULL, *cachePosition = NULL;
    for(int i = 0; i<INVOKATION_CACHE_SIZE; i++) { 
      /* Fast cache access */
      entry = &(method->invokations[i]);
      if(entry->fptr &&
         entry->signature[0] == argSignature[0] && 
         entry->signature[1] == argSignature[1] && 
         entry->signature[2] == argSignature[2] && 
         entry->packed == packedArg &&
         entry->returnType == retValType) return entry->fptr;
      if(!entry->fptr) cachePosition = entry;
    } 

    std::vector<ObjectTypeSet> argT;

    ObjectTypeSet retValT;
    uint64_t mask = 0xff;

    for(uint64_t i = 0; i < argCount; i++) {
      int group = i / 8;
      int index = i % 8;
      uint64_t type = (argSignature[group] >> (8 * index)) & mask;
      unsigned char isBoxed = (packedArg >> i) & 1;
      argT.insert(argT.begin(), ObjectTypeSet((objectType)type, isBoxed));
    }

    retValT = retValType == 0 ? ObjectTypeSet::all() : ObjectTypeSet((objectType)retValType);
    std::string name = "fn_" + std::to_string(fun->uniqueId);   
    auto f = std::make_unique<FunctionJIT>();
    std::string rqName = ObjectTypeSet::fullyQualifiedMethodKey(name, argT, retValT);        
    f->args = argT;
    f->retVal = retValT;
    f->uniqueId = fun->uniqueId;
    f->methodIndex = method->index;

    /* Closed overs */ 
    addClosedOversToFunctionJIT(method, *f);

    llvm::ExitOnError eo = llvm::ExitOnError();
    /* TODO - we probably need to modify TheProgramme here somehow, this needs more thinking */
    eo(jit->addAST(std::move(f)));
    void *retVal = nullptr;
    try {
      auto ExprSymbol = eo(jit->lookup(rqName));
      retVal = (void *)ExprSymbol.getAddress();
      
    } catch(CodeGenerationException e) {
      printf("Code generation exception detected in DynamicRuntime.cpp/specialiseDynamicFn !!!! %s\n", e.toString().c_str());
      throw e;
    }
    if(cachePosition) { /* Store in cache if free entries available */
      cachePosition->signature[0] = argSignature[0];
      cachePosition->signature[1] = argSignature[1];
      cachePosition->signature[2] = argSignature[2];
      cachePosition->packed = packedArg;
      cachePosition->returnType = retValType;
      cachePosition->fptr = retVal;
    }
    return retVal;
  }
}
