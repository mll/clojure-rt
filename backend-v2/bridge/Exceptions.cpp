#include "Exceptions.h"
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdio.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <limits.h>
#include <unistd.h>
#endif

namespace rt {

LanguageException::LanguageException(const std::string &name, RTValue message,
                                     RTValue payload) {
  this->payload = payload;
  this->name = name;
  this->message = message;
  void *buffer[128];
  int size = backtrace(buffer, 128);
  stackAddresses.reserve(size);
  for (int i = 0; i < size; i++) {
    stackAddresses.push_back(reinterpret_cast<uword_t>(buffer[i]));
  }
}

void LanguageException::printRawTrace() const {
  for (uword_t addr : stackAddresses) {
    printf("  [JIT ADDR] %p\n", (void *)addr);
  }
}

std::string
LanguageException::toString(llvm::symbolize::LLVMSymbolizer &symbolizer,
                            const std::string &moduleName,
                            const intptr_t slide) const {
  std::stringstream ss;

  ss << "Exception: " << name << "\n";
  retain(message);
  String *messageString = String_compactify(::toString(message));
  ss << "Message: " << String_c_str(messageString) << "\n";
  Ptr_release(messageString);

  retain(payload);
  String *payloadString = String_compactify(::toString(payload));
  ss << "Payload: " << String_c_str(payloadString) << "\n";
  Ptr_release(payloadString);

  ss << "Stack Trace:\n";
  bool found = false;
  for (uword_t addr : stackAddresses) {

    uword_t lookupAddr = addr - slide;

    auto resOrErr = symbolizer.symbolizeCode(
        moduleName, {lookupAddr, llvm::object::SectionedAddress::UndefSection});

    if (resOrErr) {
      auto &info = resOrErr.get();
      if (!found &&
          info.FunctionName == "throwInternalInconsistencyException") {
        found = true;
        continue;
      }
      if (found) {
        ss << "  at ";

        if (info.FunctionName != "<invalid>") {
          ss << info.FunctionName;
        } else {
          ss << "0x" << std::hex << std::setw(sizeof(uword_t) * 2)
             << std::setfill('0') << addr << std::dec;
        }
        if (info.FileName != "<invalid>") {
          ss << " [" << info.FileName << ":" << info.Line << "]";
        }
      }
    } else {
      if (found) {
        ss << "  at ";
        ss << "?? [0x" << std::hex << addr << std::dec << "]";
        llvm::consumeError(resOrErr.takeError());
      }
    }
    if (found)
      ss << "\n";
  }

  return ss.str();
}

std::string getSelfExecutablePath() {
#ifdef __APPLE__
  char path[1024];
  uint32_t size = sizeof(path);
  if (_NSGetExecutablePath(path, &size) == 0)
    return std::string(path);
#elif defined(__linux__)
  char path[PATH_MAX];
  ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
  if (count != -1) {
    return std::string(path, count);
  }
#endif
  return "";
}

std::string getExceptionString(const LanguageException &e) {
  llvm::symbolize::LLVMSymbolizer::Options options;
  options.Demangle = true;
  options.PrintFunctions = llvm::symbolize::FunctionNameKind::LinkageName;
  llvm::symbolize::LLVMSymbolizer symbolizer(options);

  std::string exePath = getSelfExecutablePath();
  std::string moduleName = exePath;

#ifdef __APPLE__
  size_t lastSlash = exePath.find_last_of('/');
  std::string basename = (lastSlash != std::string::npos)
                             ? exePath.substr(lastSlash + 1)
                             : "clojure-rt";
  moduleName = exePath + ".dSYM/Contents/Resources/DWARF/" + basename;
  intptr_t slide = _dyld_get_image_vmaddr_slide(0);
#else
  intptr_t slide = 0;
#endif

  return e.toString(symbolizer, moduleName, slide);
}

} // namespace rt

extern "C" {
void throwInternalInconsistencyException(const std::string &errorMessage) {
  throw rt::LanguageException(
      "InternalInconsistencyException",
      RT_boxPtr(String_createDynamicStr(errorMessage.c_str())), RT_boxNil());
}

void throwCodeGenerationException(const std::string &errorMessage,
                                  const Node &node) {
  std::stringstream retval;
  auto env = node.env();
  retval << env.file() << ":" << env.line() << ":" << env.column()
         << ": error: " << errorMessage << "\n";
  retval << node.form() << "\n";
  retval << std::string(node.form().length(), '^') << "\n";
  throw rt::LanguageException(
      "CodeGenerationException",
      RT_boxPtr(String_createDynamicStr(retval.str().c_str())), RT_boxNil());
}
}
