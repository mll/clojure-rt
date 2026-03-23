#ifndef RT_EXCEPTIONS
#define RT_EXCEPTIONS

#include "bytecode.pb.h"
#include "llvm/DebugInfo/Symbolize/Symbolize.h"
#include "llvm/Support/Error.h"
#include <sstream>

#include "../RuntimeHeaders.h"
#include "../runtime/Exceptions.h"
#include <gmp.h>

#include <exception>
#include <execinfo.h>
#include <string>
#include <vector>

using namespace clojure::rt::protobuf::bytecode;

namespace rt {

enum class StackTraceMode { Friendly, Debug };

struct CapturedStack {
  std::vector<uword_t> addresses;
  std::shared_ptr<CapturedStack> parent;
};

extern thread_local std::shared_ptr<CapturedStack> gCurrentAsyncStack;

std::shared_ptr<CapturedStack> captureCurrentStack();

class LanguageException : public std::exception {
  std::string name;
  RTValue message;
  RTValue payload;
  std::shared_ptr<CapturedStack> capturedStack;

public:
  LanguageException(const std::string &name, RTValue message, RTValue payload);
  LanguageException(const LanguageException &other);
  LanguageException &operator=(const LanguageException &other);
  ~LanguageException() noexcept override;
  void printRawTrace() const;
  
  std::string toString(llvm::symbolize::LLVMSymbolizer &symbolizer,
                       const std::string &moduleName = "JITMemoryBuffer",
                       const intptr_t slide = 0x0,
                       StackTraceMode mode = StackTraceMode::Friendly) const;
};

std::string getExceptionString(const LanguageException &e,
                               StackTraceMode mode = StackTraceMode::Friendly);

} // namespace rt

// C++-only functions (no extern "C" needed/possible due to std::string/Node)
[[noreturn]] void
throwInternalInconsistencyException(const std::string &errorMessage);
[[noreturn]] void throwCodeGenerationException(const std::string &errorMessage,
                                               const Node &node);

extern "C" void registerJitFunction_C(uword_t addr, size_t size,
                                      const char *name, const void *objData,
                                      size_t objSize);

extern "C" [[noreturn]] void
throwNoMatchingOverloadException_C(const char *className,
                                   const char *methodName);

#endif
