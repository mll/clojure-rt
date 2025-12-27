#ifndef RT_EXCEPTIONS
#define RT_EXCEPTIONS

#include "bytecode.pb.h"
#include <sstream>
#include "llvm/DebugInfo/Symbolize/Symbolize.h"
#include "llvm/Support/Error.h"


#include <gmp.h>

extern "C" {

#ifdef __cplusplus
  // Jeśli kompilujemy jako C++, zamień 'restrict' na wersję rozumianą przez kompilator
  #if defined(__GNUC__) || defined(__clang__)
    #define restrict __restrict__
  #elif defined(_MSC_VER)
    #define restrict __restrict
  #else
    #define restrict
  #endif
#endif

#include "../runtime/RTValue.h"
#include "../runtime/word.h"
#include "../runtime/ObjectProto.h"
#include "../runtime/Object.h"
#include "../runtime/String.h"
}

#include <execinfo.h> 
#include <vector>
#include <string>
#include <exception>

using namespace clojure::rt::protobuf::bytecode;

namespace rt {

class LanguageException : public std::exception {
  std::string name;
  RTValue message;  
  RTValue payload;
  std::vector<uword_t> stackAddresses; 
public:        
  LanguageException(const std::string &name, RTValue message, RTValue payload);
  void printRawTrace() const;
  std::string toString(llvm::symbolize::LLVMSymbolizer &symbolizer,
                       const std::string &moduleName = "JITMemoryBuffer",
                       const intptr_t slide = 0x0) const;
};

}

extern "C" {
  void throwInternalInconsistencyException(const std::string &errorMessage);
  void throwCodeGenerationException(const std::string &errorMessage, const Node &node);
}


#endif
