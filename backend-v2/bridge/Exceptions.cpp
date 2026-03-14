#include "Exceptions.h"
#include "../runtime/Exceptions.h"
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/MemoryBuffer.h>
#include <stdio.h>

#ifdef __APPLE__
#include <dlfcn.h>
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <dlfcn.h>
#include <limits.h>
#include <unistd.h>
#endif

#include <map>
#include <mutex>

namespace rt {

struct JitInfo {
  std::string name;
  size_t size;
  std::unique_ptr<llvm::MemoryBuffer> objectBuffer;
};

static std::mutex gJitMapMutex;
static std::map<uword_t, JitInfo> gJitFunctions;

extern "C" void registerJitFunction_C(uword_t addr, size_t size,
                                      const char *name, const void *objData,
                                      size_t objSize) {
  std::lock_guard<std::mutex> lock(gJitMapMutex);
  auto buffer = llvm::MemoryBuffer::getMemBufferCopy(
      llvm::StringRef(static_cast<const char *>(objData), objSize),
      name ? name : "jit_object");
  gJitFunctions[addr] = {name ? name : "", size, std::move(buffer)};
}

// Removing redundant namespace rt {

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

LanguageException::LanguageException(const LanguageException &other) {
  this->name = other.name;
  this->message = other.message;
  this->payload = other.payload;
  this->stackAddresses = other.stackAddresses;
  retain(this->message);
  retain(this->payload);
}

LanguageException &
LanguageException::operator=(const LanguageException &other) {
  if (this != &other) {
    release(this->message);
    release(this->payload);
    this->name = other.name;
    this->message = other.message;
    this->payload = other.payload;
    this->stackAddresses = other.stackAddresses;
    retain(this->message);
    retain(this->payload);
  }
  return *this;
}

LanguageException::~LanguageException() noexcept {
  release(message);
  release(payload);
}

void LanguageException::printRawTrace() const {
  for (uword_t addr : stackAddresses) {
    printf("  [JIT ADDR] %p\n", (void *)addr);
  }
}

std::string
LanguageException::toString(llvm::symbolize::LLVMSymbolizer &symbolizer,
                            const std::string &defaultModuleName,
                            const intptr_t defaultSlide) const {
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
    std::string currentModule = "";
    uword_t lookupAddr = addr;
    uword_t symbolizeAddr = 0;
    std::string dlSymbolName = "";

    Dl_info dlinfo;
    bool hasDlInfo = dladdr(reinterpret_cast<void *>(addr), &dlinfo);
    if (hasDlInfo) {
      if (dlinfo.dli_sname) {
        dlSymbolName = dlinfo.dli_sname;
      }

      // 1. Check if we should start the trace (Found logic)
      if (!found && !dlSymbolName.empty() &&
          dlSymbolName.find("throw") != std::string::npos &&
          dlSymbolName.find("Exception") != std::string::npos) {
        found = true;
        continue;
      }

#ifdef __APPLE__
      // 2. Identify module and offset
      if (dlinfo.dli_fbase == _dyld_get_image_header(0)) {
        // Main executable - must use dSYM and slide
        currentModule = defaultModuleName;
        lookupAddr = addr - defaultSlide;
      } else if (dlinfo.dli_fname) {
        currentModule = dlinfo.dli_fname;
        lookupAddr = addr - reinterpret_cast<uword_t>(dlinfo.dli_fbase);
      }
#else
      if (dlinfo.dli_fname) {
        currentModule = dlinfo.dli_fname;
        lookupAddr = addr - reinterpret_cast<uword_t>(dlinfo.dli_fbase);
      }
#endif
    } else {
      // Check JIT map
      std::lock_guard<std::mutex> lock(gJitMapMutex);
      auto it = gJitFunctions.lower_bound(addr);
      if (it != gJitFunctions.begin()) {
        auto prev = std::prev(it);
        if (addr >= prev->first && addr < prev->first + prev->second.size) {
          if (prev->second.objectBuffer) {
            uword_t jitSymbolizeAddr = addr - prev->first;
#if defined(__aarch64__) || defined(__arm64__)
            if (jitSymbolizeAddr >= 4)
              jitSymbolizeAddr -= 4;
#else
            if (jitSymbolizeAddr > 0)
              jitSymbolizeAddr -= 1;
#endif
            auto ObjOrErr = llvm::object::ObjectFile::createObjectFile(
                prev->second.objectBuffer->getMemBufferRef());
            if (ObjOrErr) {
              auto &Obj = *ObjOrErr.get();
              auto resOrErrJit = symbolizer.symbolizeCode(
                  Obj, {jitSymbolizeAddr,
                        llvm::object::SectionedAddress::UndefSection});
              if (resOrErrJit) {
                auto &info = resOrErrJit.get();
                if (info.FileName != "<invalid>") {
                  ss << "  at " << info.FunctionName << " [" << info.FileName
                     << ":" << info.Line << "]\n";
                  found = true;
                  continue;
                }
              } else {
                llvm::consumeError(resOrErrJit.takeError());
              }
            } else {
              llvm::consumeError(ObjOrErr.takeError());
            }
          }

          if (found) {
            ss << "  at " << prev->second.name << " [JIT:0x" << std::hex
               << prev->first << std::dec << "]\n";
          }
          continue;
        }
      }
    }

    if (!found)
      continue;

    // Adjust address back to get the call site line instead of return site.
    symbolizeAddr = lookupAddr;
#if defined(__aarch64__) || defined(__arm64__)
    if (symbolizeAddr >= 4)
      symbolizeAddr -= 4;
#else
    if (symbolizeAddr > 0)
      symbolizeAddr -= 1;
#endif

    if (currentModule.empty()) {
      currentModule = defaultModuleName;
#if defined(__aarch64__) || defined(__arm64__)
      symbolizeAddr = (addr - defaultSlide) - 4;
#else
      symbolizeAddr = (addr - defaultSlide) - 1;
#endif
    }

    auto resOrErr = symbolizer.symbolizeCode(
        currentModule,
        {symbolizeAddr, llvm::object::SectionedAddress::UndefSection});

    if (resOrErr) {
      auto &info = resOrErr.get();
      // Second chance for found (if dladdr failed somehow)
      if (!found &&
          (info.FunctionName.find("throw") != std::string::npos &&
           info.FunctionName.find("Exception") != std::string::npos)) {
        found = true;
        continue;
      }

      if (found) {
        ss << "  at ";

        if (info.FunctionName != "<invalid>") {
          ss << info.FunctionName;
        } else if (!dlSymbolName.empty()) {
          ss << dlSymbolName;
        } else if (!currentModule.empty()) {
          size_t lastSlash = currentModule.find_last_of('/');
          std::string base = (lastSlash != std::string::npos)
                                 ? currentModule.substr(lastSlash + 1)
                                 : currentModule;
          ss << base << " + 0x" << std::hex << lookupAddr << std::dec;
        } else {
          ss << "0x" << std::hex << std::setw(sizeof(uword_t) * 2)
             << std::setfill('0') << addr << std::dec;
        }

        if (info.FileName != "<invalid>") {
          ss << " [" << info.FileName << ":" << info.Line << "]";
        }
        ss << "\n";
      }
    } else {
      llvm::consumeError(resOrErr.takeError());
      if (found) {
        if (!dlSymbolName.empty()) {
          ss << "  at " << dlSymbolName << " [" << currentModule << "]\n";
        } else {
          ss << "  at 0x" << std::hex << addr << std::dec << " ["
             << currentModule << "]\n";
        }
      }
    }
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

void throwInternalInconsistencyException(const std::string &errorMessage) {
  throw rt::LanguageException(
      "InternalInconsistencyException",
      RT_boxPtr(String_createDynamicStr(errorMessage.c_str())), RT_boxNil());
}

extern "C" void
throwInternalInconsistencyException_C(const char *errorMessage) {
  throw rt::LanguageException("InternalInconsistencyException",
                              RT_boxPtr(String_createDynamicStr(errorMessage)),
                              RT_boxNil());
}

extern "C" void throwNoMatchingOverloadException_C(const char *className,
                                                   const char *methodName) {
  std::stringstream ss;
  ss << "No matching overload found for " << className << "/" << methodName;
  throw rt::LanguageException(
      "NoMatchingOverloadException",
      RT_boxPtr(String_createDynamicStr(ss.str().c_str())), RT_boxNil());
}

extern "C" void throwLanguageException_C(const char *name, RTValue message,
                                         RTValue payload) {
  throw rt::LanguageException(name, message, payload);
}

extern "C" void throwArityException_C(int expected, int actual) {
  std::stringstream ss;
  ss << "Wrong number of args (" << actual << ") passed to function";
  throw rt::LanguageException(
      "ArityException", RT_boxPtr(String_createDynamicStr(ss.str().c_str())),
      RT_boxNil());
}

extern "C" void throwIllegalArgumentException_C(const char *message) {
  throw rt::LanguageException("IllegalArgumentException",
                              RT_boxPtr(String_createDynamicStr(message)),
                              RT_boxNil());
}

extern "C" void throwIllegalStateException_C(const char *message) {
  throw rt::LanguageException("IllegalStateException",
                              RT_boxPtr(String_createDynamicStr(message)),
                              RT_boxNil());
}

extern "C" void throwUnsupportedOperationException_C(const char *message) {
  throw rt::LanguageException("UnsupportedOperationException",
                              RT_boxPtr(String_createDynamicStr(message)),
                              RT_boxNil());
}

extern "C" void throwArithmeticException_C(const char *message) {
  throw rt::LanguageException("ArithmeticException",
                              RT_boxPtr(String_createDynamicStr(message)),
                              RT_boxNil());
}

extern "C" void throwIndexOutOfBoundsException_C(uword_t index, uword_t count) {
  std::stringstream ss;
  ss << "Index out of bounds: " << index << " (count: " << count << ")";
  throw rt::LanguageException(
      "IndexOutOfBoundsException",
      RT_boxPtr(String_createDynamicStr(ss.str().c_str())), RT_boxNil());
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
