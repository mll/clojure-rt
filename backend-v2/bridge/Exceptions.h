#ifndef RT_EXCEPTIONS
#define RT_EXCEPTIONS

#include "bytecode.pb.h"
#include "llvm/DebugInfo/Symbolize/Symbolize.h"
#include "llvm/Support/Error.h"
#include <sstream>

#include "../RuntimeHeaders.h"
#include <gmp.h>

#include <exception>
#include <execinfo.h>
#include <string>
#include <vector>

using namespace clojure::rt::protobuf::bytecode;

namespace rt {

class LanguageException : public std::exception {
  std::string name;
  RTValue message;
  RTValue payload;
  std::vector<uword_t> stackAddresses;

public:
  LanguageException(const std::string &name, RTValue message, RTValue payload);
  LanguageException(const LanguageException &other);
  LanguageException &operator=(const LanguageException &other);
  ~LanguageException() noexcept override;
  void printRawTrace() const;
  std::string toString(llvm::symbolize::LLVMSymbolizer &symbolizer,
                       const std::string &moduleName = "JITMemoryBuffer",
                       const intptr_t slide = 0x0) const;
};

std::string getExceptionString(const LanguageException &e);

} // namespace rt

// C++-only functions (no extern "C" needed/possible due to std::string/Node)
[[noreturn]] void
throwInternalInconsistencyException(const std::string &errorMessage);
[[noreturn]] void throwCodeGenerationException(const std::string &errorMessage,
                                               const Node &node);

extern "C" void registerJitFunction_C(uword_t addr, size_t size,
                                      const char *name,
                                      const void *objData, size_t objSize);

extern "C" [[noreturn]] void
throwInternalInconsistencyException_C(const char *errorMessage);

extern "C" [[noreturn]] void
throwNoMatchingOverloadException_C(const char *className, const char *methodName);

#endif
