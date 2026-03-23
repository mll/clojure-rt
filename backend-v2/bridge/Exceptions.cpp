#include "Exceptions.h"
#include "../runtime/Exceptions.h"
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <llvm/Demangle/Demangle.h>
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

thread_local std::shared_ptr<CapturedStack> gCurrentAsyncStack = nullptr;

struct JitInfo {
  std::string name;
  size_t size;
  std::unique_ptr<llvm::MemoryBuffer> objectBuffer;
  mutable std::unique_ptr<llvm::object::ObjectFile> objectFile;
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
  gJitFunctions[addr] = {name ? name : "", size, std::move(buffer), nullptr};
}

std::shared_ptr<CapturedStack> captureCurrentStack() {
  auto stack = std::make_shared<CapturedStack>();
  void *buffer[128];
  int size = backtrace(buffer, 128);
  stack->addresses.reserve(size);
  // Skip 2 frames to bypass captureCurrentStack and its direct caller (wrapper)
  for (int i = 2; i < size; i++) {
    stack->addresses.push_back(reinterpret_cast<uword_t>(buffer[i]));
  }
  stack->parent = gCurrentAsyncStack;
  return stack;
}

LanguageException::LanguageException(const std::string &name, RTValue message,
                                     RTValue payload) {
  this->payload = payload;
  this->name = name;
  this->message = message;
  this->capturedStack = captureCurrentStack();
}

LanguageException::LanguageException(const LanguageException &other) {
  this->name = other.name;
  this->message = other.message;
  this->payload = other.payload;
  this->capturedStack = other.capturedStack;
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
    this->capturedStack = other.capturedStack;
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
  if (!capturedStack) return;
  for (uword_t addr : capturedStack->addresses) {
    printf("  [JIT ADDR] %p\n", (void *)addr);
  }
}

static bool isInfrastructureFrame(const std::string &fnName,
                                   const std::string &fileName) {
  if (fnName.empty())
    return false;

  // Patterns to skip in Friendly mode
  static const char *skipPatterns[] = {
      "std::__1",
      "rt::InvokeManager",
      "rt::CodeGen",
      "rt::JITEngine",
      "rt::ThreadPool",
      "rt::CleanupChain",
      "rt::InstanceCallStub",
      "InstanceCallSlowPath",
      "throwInternalInconsistencyException",
      "throwNoMatchingOverloadException",
      "throwLanguageException",
      "throwArityException",
      "throwIllegalArgumentException",
      "throwIllegalStateException",
      "throwUnsupportedOperationException",
      "throwArithmeticException",
      "throwIndexOutOfBoundsException",
      "throwCodeGenerationException",
      "asan_thread_start",
      "__packaged_task",
      "__invoke",
      "__function",
      "__alloc_func",
      "__value_func",
      "__packaged_task_func",
      "__thread_proxy",
      "__thread_execute",
      "future",
      "decltype",
      "std::declval",
      "cmocka_",
      "_cmocka_"};

  for (const char *p : skipPatterns) {
    if (fnName.find(p) != std::string::npos)
      return true;
  }

  // System headers / ASan libs / C++ machinery
  if (fileName.find("/usr/include/c++/") != std::string::npos)
    return true;
  if (fileName.find("libclang_rt.asan") != std::string::npos)
    return true;
  if (fileName.find("__functional") != std::string::npos)
    return true;
  if (fileName.find("__type_traits") != std::string::npos)
    return true;
  if (fileName.find("/c++/v1/") != std::string::npos)
    return true;

  return false;
}

static void symbolizeStackChain(std::stringstream &ss,
                               const std::shared_ptr<CapturedStack> &stack,
                               llvm::symbolize::LLVMSymbolizer &symbolizer,
                               const std::string &moduleName,
                               const intptr_t slide, StackTraceMode mode) {
  if (!stack) return;

  auto current = stack;
  bool isFirst = true;
  while (current) {
    bool found = (mode == StackTraceMode::Debug) || !isFirst;

    for (uword_t addr : current->addresses) {
      std::string currentModule = "";
      uword_t lookupAddr = addr;
      uword_t symbolizeAddr = 0;
      std::string dlSymbolName = "";
      bool addrHandled = false;

      // 1. Check JIT map first
      {
        std::lock_guard<std::mutex> lock(gJitMapMutex);
        auto it = gJitFunctions.lower_bound(addr);
        if (it != gJitFunctions.begin() || (it != gJitFunctions.end() && it->first == addr)) {
          auto entry = (it != gJitFunctions.end() && it->first == addr) ? it : std::prev(it);
          if (addr >= entry->first && addr < entry->first + entry->second.size) {
            if (entry->second.objectBuffer) {
              if (!entry->second.objectFile) {
                auto ObjOrErr = llvm::object::ObjectFile::createObjectFile(
                    entry->second.objectBuffer->getMemBufferRef());
                if (ObjOrErr) {
                  entry->second.objectFile = std::move(ObjOrErr.get());
                } else {
                  llvm::consumeError(ObjOrErr.takeError());
                }
              }

              if (entry->second.objectFile) {
                uword_t jitSymbolizeAddr = addr - entry->first;
#if defined(__aarch64__) || defined(__arm64__)
                if (jitSymbolizeAddr >= 4) jitSymbolizeAddr -= 4;
#else
                if (jitSymbolizeAddr > 0) jitSymbolizeAddr -= 1;
#endif
                auto &Obj = *entry->second.objectFile;
                auto resOrErrJit = symbolizer.symbolizeCode(Obj, {jitSymbolizeAddr, llvm::object::SectionedAddress::UndefSection});
                if (resOrErrJit) {
                  auto &info = resOrErrJit.get();
                  if (info.FileName != "<invalid>") {
                    std::string demangled = llvm::demangle(info.FunctionName);
                    bool isInfra = isInfrastructureFrame(demangled, info.FileName);
                    if (!found && !isInfra) found = true;
                    if (found && (mode == StackTraceMode::Debug || !isInfra)) {
                      ss << "  at " << demangled << " [" << info.FileName << ":" << info.Line << "]\n";
                      addrHandled = true;
                    } else if (isInfra && !found) {
                      // Skip infra at the top
                      addrHandled = true;
                    }
                  }
                } else {
                  llvm::consumeError(resOrErrJit.takeError());
                }
              }
            }

            if (!addrHandled && !entry->second.name.empty()) {
              std::string demangled = llvm::demangle(entry->second.name);
              bool isInfra = isInfrastructureFrame(demangled, "");
              if (!found && !isInfra) found = true;
              if (found && (mode == StackTraceMode::Debug || !isInfra)) {
                ss << "  at " << demangled << " [JIT:0x" << std::hex << entry->first << std::dec << "]\n";
                addrHandled = true;
              } else if (isInfra && !found) {
                addrHandled = true;
              }
            }
            if (addrHandled) continue;
          }
        }
      }

      Dl_info dlinfo;
      bool hasDlInfo = dladdr(reinterpret_cast<void *>(addr), &dlinfo);
      if (hasDlInfo) {
        if (dlinfo.dli_sname) dlSymbolName = dlinfo.dli_sname;

        // Skip thread entry points if we have a parent to stitch to
        if (current->parent && (dlSymbolName == "thread_start" || dlSymbolName == "_pthread_start" || dlSymbolName == "start" || dlSymbolName == "asan_thread_start")) {
          continue;
        }

        if (!found) {
          std::string demangled = llvm::demangle(dlSymbolName);
          if (!isInfrastructureFrame(demangled, dlinfo.dli_fname ? dlinfo.dli_fname : "")) {
            found = true;
          }
        }

#ifdef __APPLE__
        if (dlinfo.dli_fbase == _dyld_get_image_header(0)) {
          currentModule = moduleName;
          lookupAddr = addr - slide;
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
      }

      if (!found) continue;

      symbolizeAddr = lookupAddr;
#if defined(__aarch64__) || defined(__arm64__)
      if (symbolizeAddr >= 4) symbolizeAddr -= 4;
#else
      if (symbolizeAddr > 0) symbolizeAddr -= 1;
#endif

      if (currentModule.empty()) {
        currentModule = moduleName;
#if defined(__aarch64__) || defined(__arm64__)
        symbolizeAddr = (addr - slide) - 4;
#else
        symbolizeAddr = (addr - slide) - 1;
#endif
      }

      auto resOrErr = symbolizer.symbolizeCode(currentModule, {symbolizeAddr, llvm::object::SectionedAddress::UndefSection});
      if (resOrErr) {
        auto &info = resOrErr.get();
        std::string rawFn = (info.FunctionName != "<invalid>") ? info.FunctionName : dlSymbolName;
        std::string demangled = llvm::demangle(rawFn);
        bool isInfra = isInfrastructureFrame(demangled, info.FileName);
        if (!found && !isInfra) found = true;
        if (found && (mode == StackTraceMode::Debug || !isInfra)) {
          ss << "  at " << demangled;
          if (info.FileName != "<invalid>") ss << " [" << info.FileName << ":" << info.Line << "]";
          else if (!currentModule.empty()) {
            size_t lastSlash = currentModule.find_last_of('/');
            std::string base = (lastSlash != std::string::npos) ? currentModule.substr(lastSlash + 1) : currentModule;
            ss << " [" << base << " + 0x" << std::hex << lookupAddr << std::dec << "]";
          }
          ss << "\n";
        }
      } else {
        llvm::consumeError(resOrErr.takeError());
        std::string demangled = llvm::demangle(dlSymbolName);
        bool isInfra = isInfrastructureFrame(demangled, currentModule);
        if (!found && !isInfra) found = true;
        if (found && (mode == StackTraceMode::Debug || !isInfra)) {
          if (!dlSymbolName.empty()) ss << "  at " << demangled << " [" << currentModule << "]\n";
          else ss << "  at 0x" << std::hex << addr << std::dec << " [" << currentModule << "]\n";
        }
      }
    }
    
    current = current->parent;
    isFirst = false;
  }
}

std::string
LanguageException::toString(llvm::symbolize::LLVMSymbolizer &symbolizer,
                            const std::string &defaultModuleName,
                            const intptr_t defaultSlide,
                            StackTraceMode mode) const {
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
  symbolizeStackChain(ss, capturedStack, symbolizer, defaultModuleName, defaultSlide, mode);

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

std::string getExceptionString(const LanguageException &e, StackTraceMode mode) {
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

  return e.toString(symbolizer, moduleName, slide, mode);
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
