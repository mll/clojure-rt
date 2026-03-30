#ifndef RT_EXCEPTIONS
#define RT_EXCEPTIONS

#include "llvm/DebugInfo/Symbolize/Symbolize.h"
#include "llvm/Support/Error.h"
#include <sstream>

#include "../RuntimeHeaders.h"
#include "../runtime/Exceptions.h"
#include <gmp.h>

#include "SourceLocation.h"
#include <exception>
#include <execinfo.h>
#include <string>
#include <vector>

namespace clojure {
namespace rt {
namespace protobuf {
namespace bytecode {
class Node;
}
} // namespace protobuf
} // namespace rt
} // namespace clojure

namespace rt {

enum class StackTraceMode { Friendly, Debug };

struct JitFunctionEntry {
  uword_t size;
  std::string name;
  std::vector<uint8_t> objectData;
  std::map<SourceLocation, std::string> locationToForm;
};

struct CapturedStack {
  std::vector<uword_t> addresses;
  std::vector<std::string> symbolizedFrames;
  std::shared_ptr<CapturedStack> parent;
};

extern thread_local std::shared_ptr<CapturedStack> gCurrentAsyncStack;

std::shared_ptr<CapturedStack> captureCurrentStack();

class LanguageException : public std::exception {
  std::string name;
  RTValue message;
  RTValue payload;
  std::string form;
  std::string sourceLocation;
  std::shared_ptr<CapturedStack> capturedStack;
  mutable std::string cachedMessage;

public:
  LanguageException(const std::string &name, RTValue message, RTValue payload);
  LanguageException(const std::string &name, RTValue message, RTValue payload,
                    const std::string &form, const std::string &sourceLocation);
  LanguageException(const LanguageException &other);
  LanguageException &operator=(const LanguageException &other);
  ~LanguageException() noexcept override;
  void printRawTrace() const;
  const char *what() const noexcept override;

  std::string toString(llvm::symbolize::LLVMSymbolizer &symbolizer,
                       const std::string &moduleName = "JITMemoryBuffer",
                       const intptr_t slide = 0x0,
                       StackTraceMode mode = StackTraceMode::Friendly,
                       bool useColor = true) const;

  const std::string &getName() const { return name; }
  RTValue getMessage() const { return message; }
  RTValue getPayload() const { return payload; }
  const std::string &getForm() const { return form; }
  const std::string &getSourceLocation() const { return sourceLocation; }
  std::shared_ptr<CapturedStack> getCapturedStack() const {
    return capturedStack;
  }
};

std::string getExceptionString(const LanguageException &e,
                               StackTraceMode mode = StackTraceMode::Friendly,
                               bool useColor = true);

[[noreturn]] void
throwInternalInconsistencyException(const std::string &errorMessage);
[[noreturn]] void throwCodeGenerationException(const std::string &errorMessage,
                                               const std::string &form,
                                               const std::string &file,
                                               int line, int column);
[[noreturn]] void
throwCodeGenerationException(const std::string &errorMessage,
                             const clojure::rt::protobuf::bytecode::Node &node);

// New registration function with form mapping
void registerJitFunction(
    uword_t addr, size_t size, const char *name, const void *objData,
    size_t objSize, std::map<rt::SourceLocation, std::string> locationToForm);

} // namespace rt

extern "C" void registerJitFunction_C(uword_t addr, size_t size,
                                      const char *name, const void *objData,
                                      size_t objSize);

extern "C" [[noreturn]] void
throwNoMatchingOverloadException_C(const char *className,
                                   const char *methodName);

extern "C" String *exceptionToString_C(void *exception);
extern "C" void *createException_C(const char *className, String *message,
                                   RTValue payload);
extern "C" [[noreturn]] void throwException_C(RTValue exceptionBoxed);
extern "C" void deleteException_C(void *exception);

#endif
