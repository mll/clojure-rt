#include "Exceptions.h"
#include "llvm/DebugInfo/Symbolize/Symbolize.h"
#include "llvm/Demangle/Demangle.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/Error.h"
#include <dlfcn.h>
#include <execinfo.h>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace rt {

thread_local std::shared_ptr<CapturedStack> gCurrentAsyncStack = nullptr;

struct JitFunctionEntry {
  uword_t size;
  std::string name;
  std::unique_ptr<llvm::MemoryBuffer> objectBuffer;
  mutable std::unique_ptr<llvm::object::ObjectFile> objectFile;
};

static std::map<uword_t, JitFunctionEntry> gJitFunctions;
static std::mutex gJitMapMutex;

void registerJitFunction(uword_t address, uword_t size, const char *name,
                         const void *objectData, size_t objectSize) {
  std::lock_guard<std::mutex> lock(gJitMapMutex);
  JitFunctionEntry entry;
  entry.size = size;
  entry.name = name;
  if (objectData && objectSize > 0) {
    entry.objectBuffer = llvm::MemoryBuffer::getMemBufferCopy(llvm::StringRef(
        reinterpret_cast<const char *>(objectData), objectSize));
  }
  gJitFunctions[address] = std::move(entry);
}

extern "C" void registerJitFunction_C(uword_t address, uword_t size,
                                      const char *name, const void *objectData,
                                      size_t objectSize) {
  registerJitFunction(address, size, name, objectData, objectSize);
}

static bool isInfrastructureFrame(const std::string &name,
                                  const std::string &file) {
  static const std::vector<std::string> skipPatterns = {
      "std::__1",
      "rt::InvokeManager",
      "rt::CodeGen",
      "rt::JITEngine",
      "rt::ThreadPool",
      "rt::LanguageException",
      "captureCurrentStack",
      "throwArithmeticException_C",
      "throwInternalInconsistencyException",
      "throwIllegalArgumentException",
      "throwNoMatchingOverloadException",
      "InstanceCallSlowPath",
      "asan_",
      "__asan_",
      "cmocka_",
      "_cmocka_",
      "libclang_rt.asan"};

  for (const auto &pattern : skipPatterns) {
    if (name.find(pattern) != std::string::npos)
      return true;
  }

  if (file.find("codegen/invoke/InvokeManager.") != std::string::npos ||
      file.find("codegen/CodeGen.") != std::string::npos ||
      file.find("jit/JITEngine.") != std::string::npos) {
    return true;
  }

  return false;
}

void symbolizeStackChain(std::stringstream &ss,
                         std::shared_ptr<CapturedStack> stack,
                         llvm::symbolize::LLVMSymbolizer &symbolizer,
                         const std::string &defaultModuleName,
                         const intptr_t defaultSlide, StackTraceMode mode) {
  auto current = stack;
  bool isFirstStack = true;
  std::string lastLine = "";

  while (current) {
    bool foundFirstUserFrame = (mode == StackTraceMode::Debug) || !isFirstStack;

    for (uword_t addr : current->addresses) {
      bool addrHandled = false;
      std::vector<std::string> currentAddrLines;

      // 1. Check JIT map
      {
        std::lock_guard<std::mutex> lock(gJitMapMutex);
        auto it = gJitFunctions.lower_bound(addr);
        if (it != gJitFunctions.begin() ||
            (it != gJitFunctions.end() && it->first == addr)) {
          auto &entry = (it != gJitFunctions.end() && it->first == addr)
                            ? it->second
                            : std::prev(it)->second;
          uword_t entryStart = (it != gJitFunctions.end() && it->first == addr)
                                   ? it->first
                                   : std::prev(it)->first;

          if (addr >= entryStart && addr < entryStart + entry.size) {
            uword_t jitSymbolizeAddr = addr - entryStart;
#if defined(__aarch64__) || defined(__arm64__)
            if (jitSymbolizeAddr >= 4)
              jitSymbolizeAddr -= 4;
#else
            if (jitSymbolizeAddr > 0)
              jitSymbolizeAddr -= 1;
#endif

            bool jitInliningProcessed = false;
            if (entry.objectBuffer) {
              if (!entry.objectFile) {
                auto ObjOrErr = llvm::object::ObjectFile::createObjectFile(
                    entry.objectBuffer->getMemBufferRef());
                if (ObjOrErr)
                  entry.objectFile = std::move(ObjOrErr.get());
                else
                  llvm::consumeError(ObjOrErr.takeError());
              }

              if (entry.objectFile) {
                auto resOrErrJit = symbolizer.symbolizeInlinedCode(
                    *entry.objectFile,
                    {jitSymbolizeAddr,
                     llvm::object::SectionedAddress::UndefSection});
                if (resOrErrJit) {
                  auto &inlinedInfo = resOrErrJit.get();
                  for (uint32_t i = 0; i < inlinedInfo.getNumberOfFrames();
                       ++i) {
                    auto &info = inlinedInfo.getFrame(i);
                    std::string fnName = (info.FunctionName != "<invalid>")
                                             ? info.FunctionName
                                             : entry.name;
                    std::string demangled = llvm::demangle(fnName);
                    bool isInfra =
                        isInfrastructureFrame(demangled, info.FileName);

                    if (!foundFirstUserFrame && !isInfra)
                      foundFirstUserFrame = true;

                    if (foundFirstUserFrame && !isInfra) {
                      std::stringstream frameSs;
                      frameSs << "  at " << demangled;
                      if (info.FileName != "<invalid>")
                        frameSs << " [" << info.FileName << ":" << info.Line
                                << "]";
                      else
                        frameSs << " [JIT:0x" << std::hex << entryStart
                                << std::dec << "]";
                      frameSs << "\n";
                      currentAddrLines.push_back(frameSs.str());
                    } else if (mode == StackTraceMode::Debug) {
                      std::stringstream frameSs;
                      frameSs << "  at " << demangled;
                      if (info.FileName != "<invalid>")
                        frameSs << " [" << info.FileName << ":" << info.Line
                                << "]";
                      else
                        frameSs << " [JIT:0x" << std::hex << entryStart
                                << std::dec << "]";

                      frameSs << " [+0x" << std::hex << (addr - entryStart)
                              << std::dec << "]";
                      if (i > 0)
                        frameSs << " (inlined)";
                      frameSs << " @ 0x" << std::hex << addr << std::dec;
                      frameSs << "\n";
                      currentAddrLines.push_back(frameSs.str());
                    }
                    jitInliningProcessed = true;
                  }
                } else {
                  llvm::consumeError(resOrErrJit.takeError());
                }
              }
            }

            if (!jitInliningProcessed && !entry.name.empty()) {
              std::string demangled = llvm::demangle(entry.name);
              bool isInfra = isInfrastructureFrame(demangled, "");
              if (!foundFirstUserFrame && !isInfra)
                foundFirstUserFrame = true;
              if (foundFirstUserFrame || mode == StackTraceMode::Debug) {
                std::stringstream frameSs;
                frameSs << "  at " << demangled << " [JIT:0x" << std::hex
                        << entryStart << std::dec << "]";
                if (mode == StackTraceMode::Debug) {
                  frameSs << " [+0x" << std::hex << (addr - entryStart)
                          << std::dec << "] @ 0x" << std::hex << addr
                          << std::dec;
                }
                frameSs << "\n";
                currentAddrLines.push_back(frameSs.str());
              }
            }
            addrHandled = true;
          }
        }
      }

      // 2. Check non-JIT symbols
      if (!addrHandled) {
        Dl_info dlinfo;
        bool hasDlInfo = dladdr(reinterpret_cast<void *>(addr), &dlinfo);
        std::string currentModule = "";
        uword_t symbolizeAddr = 0;
        std::string dlSymbolName = "";

        if (hasDlInfo) {
          if (dlinfo.dli_sname)
            dlSymbolName = dlinfo.dli_sname;

          // Skip thread entry points if we have a parent to stitch to
          if (current->parent &&
              (dlSymbolName == "thread_start" ||
               dlSymbolName == "_pthread_start" || dlSymbolName == "start" ||
               dlSymbolName == "asan_thread_start")) {
            continue;
          }

#ifdef __APPLE__
          if (dlinfo.dli_fbase == _dyld_get_image_header(0)) {
            currentModule = defaultModuleName;
            symbolizeAddr = addr - defaultSlide;
          } else if (dlinfo.dli_fname) {
            currentModule = dlinfo.dli_fname;
            symbolizeAddr = addr - reinterpret_cast<uword_t>(dlinfo.dli_fbase);
          }
#else
          if (dlinfo.dli_fname) {
            currentModule = dlinfo.dli_fname;
            symbolizeAddr = addr - reinterpret_cast<uword_t>(dlinfo.dli_fbase);
          }
#endif
        }

        if (symbolizeAddr >= 4)
          symbolizeAddr -= 4;

        auto resOrErr = symbolizer.symbolizeInlinedCode(
            currentModule,
            {symbolizeAddr, llvm::object::SectionedAddress::UndefSection});
        bool inliningSuccess = false;
        if (resOrErr) {
          auto &inlinedInfo = resOrErr.get();
          for (uint32_t i = 0; i < inlinedInfo.getNumberOfFrames(); ++i) {
            auto &info = inlinedInfo.getFrame(i);
            std::string rawFn = (info.FunctionName != "<invalid>")
                                    ? info.FunctionName
                                    : dlSymbolName;
            std::string demangled = llvm::demangle(rawFn);
            bool isInfra = isInfrastructureFrame(demangled, info.FileName);

            if (!foundFirstUserFrame && !isInfra)
              foundFirstUserFrame = true;

            if (foundFirstUserFrame && !isInfra) {
              std::stringstream frameSs;
              frameSs << "  at " << demangled;
              if (info.FileName != "<invalid>")
                frameSs << " [" << info.FileName << ":" << info.Line << "]";
              else if (!currentModule.empty()) {
                size_t lastSlash = currentModule.find_last_of('/');
                std::string base = (lastSlash != std::string::npos)
                                       ? currentModule.substr(lastSlash + 1)
                                       : currentModule;
                frameSs << " [" << base << "]";
              }
              frameSs << "\n";
              currentAddrLines.push_back(frameSs.str());
            } else if (mode == StackTraceMode::Debug) {
              std::stringstream frameSs;
              frameSs << "  at " << demangled;
              if (info.FileName != "<invalid>")
                frameSs << " [" << info.FileName << ":" << info.Line << "]";
              else if (!currentModule.empty()) {
                size_t lastSlash = currentModule.find_last_of('/');
                std::string base = (lastSlash != std::string::npos)
                                       ? currentModule.substr(lastSlash + 1)
                                       : currentModule;
                frameSs << " [" << base << "]";
              }
              if (i > 0)
                frameSs << " (inlined)";
              frameSs << " @ 0x" << std::hex << addr << std::dec;
              frameSs << "\n";
              currentAddrLines.push_back(frameSs.str());
            }
            inliningSuccess = true;
          }
        } else {
          llvm::consumeError(resOrErr.takeError());
        }

        if (!inliningSuccess) {
          std::string demangled = llvm::demangle(dlSymbolName);
          bool isInfra = isInfrastructureFrame(demangled, currentModule);
          if (!foundFirstUserFrame && !isInfra)
            foundFirstUserFrame = true;
          if (foundFirstUserFrame && !isInfra) {
            std::stringstream frameSs;
            if (!dlSymbolName.empty()) {
              frameSs << "  at " << demangled << " [" << currentModule << "]";
            } else {
              frameSs << "  at 0x" << std::hex << addr << std::dec << " ["
                      << currentModule << "]";
            }
            frameSs << "\n";
            currentAddrLines.push_back(frameSs.str());
          } else if (mode == StackTraceMode::Debug) {
            std::stringstream frameSs;
            if (!dlSymbolName.empty()) {
              frameSs << "  at " << demangled << " [" << currentModule << "]";
            } else {
              frameSs << "  at 0x" << std::hex << addr << std::dec << " ["
                      << currentModule << "]";
            }
            frameSs << " @ 0x" << std::hex << addr << std::dec;
            frameSs << "\n";
            currentAddrLines.push_back(frameSs.str());
          }
        }
      }

      for (const auto &line : currentAddrLines) {
        if (line != lastLine) {
          ss << line;
          lastLine = line;
        }
      }
    }
    current = current->parent;
    isFirstStack = false;
  }
}

LanguageException::LanguageException(const std::string &name, RTValue message,
                                     RTValue payload)
    : name(name), message(message), payload(payload) {
  capturedStack = captureCurrentStack();
  retain(message);
  retain(payload);
}

LanguageException::LanguageException(const LanguageException &other)
    : name(other.name), message(other.message), payload(other.payload),
      capturedStack(other.capturedStack) {
  retain(message);
  retain(payload);
}

LanguageException &
LanguageException::operator=(const LanguageException &other) {
  if (this != &other) {
    release(message);
    release(payload);
    name = other.name;
    message = other.message;
    payload = other.payload;
    capturedStack = other.capturedStack;
    retain(message);
    retain(payload);
  }
  return *this;
}

LanguageException::~LanguageException() noexcept {
  release(message);
  release(payload);
}

std::string
LanguageException::toString(llvm::symbolize::LLVMSymbolizer &symbolizer,
                            const std::string &moduleName, const intptr_t slide,
                            StackTraceMode mode) const {
  std::stringstream ss;
  ss << "Exception: " << name << "\n";

  String *rawMsg = ::toString(message);
  String *msgStr = String_compactify(rawMsg);
  ss << "Message: " << String_c_str(msgStr) << "\n";
  if (msgStr != rawMsg)
    Ptr_release(rawMsg);
  Ptr_release(msgStr);

  String *rawPay = ::toString(payload);
  String *payStr = String_compactify(rawPay);
  ss << "Payload: " << (payload == 0 ? "nil" : String_c_str(payStr)) << "\n";
  if (payStr != rawPay)
    Ptr_release(rawPay);
  Ptr_release(payStr);

  ss << "Stack Trace:\n";
  symbolizeStackChain(ss, capturedStack, symbolizer, moduleName, slide, mode);
  return ss.str();
}

std::shared_ptr<CapturedStack> captureCurrentStack() {
  auto stack = std::make_shared<CapturedStack>();
  void *buffer[128];
  int size = backtrace(buffer, 128);
  stack->addresses.reserve(size);
  // Skip 2 frames
  for (int i = 2; i < size; i++) {
    stack->addresses.push_back(reinterpret_cast<uword_t>(buffer[i]));
  }
  stack->parent = gCurrentAsyncStack;
  return stack;
}

void printReferenceCounts() {
  for (int i = 0; i < 256; i++) {
    uword_t count =
        atomic_load_explicit(&objectCount[i], std::memory_order_relaxed);
    if (count > 0) {
      printf("Type %d: %lu\n", i + 1, (unsigned long)count);
    }
  }
}

std::string getExceptionString(const LanguageException &e,
                               StackTraceMode mode) {
  static llvm::symbolize::LLVMSymbolizer symbolizer;

#ifdef __APPLE__
  char path[1024];
  uint32_t size = sizeof(path);
  _NSGetExecutablePath(path, &size);
  std::string exePath = path;
  size_t lastSlash = exePath.find_last_of('/');
  std::string basename = (lastSlash != std::string::npos)
                             ? exePath.substr(lastSlash + 1)
                             : "clojure-rt";
  std::string moduleName =
      exePath + ".dSYM/Contents/Resources/DWARF/" + basename;
  intptr_t slide = _dyld_get_image_vmaddr_slide(0);
#else
  std::string moduleName = "/proc/self/exe";
  intptr_t slide = 0;
#endif

  return e.toString(symbolizer, moduleName, slide, mode);
}

} // namespace rt

void throwInternalInconsistencyException(const std::string &errorMessage) {
  throw rt::LanguageException(
      "InternalInconsistencyException",
      RT_boxPtr(::String_createDynamicStr(errorMessage.c_str())), RT_boxNil());
}

extern "C" void
throwInternalInconsistencyException_C(const char *errorMessage) {
  throw rt::LanguageException(
      "InternalInconsistencyException",
      RT_boxPtr(::String_createDynamicStr(errorMessage)), RT_boxNil());
}

extern "C" [[noreturn]] void
throwNoMatchingOverloadException_C(const char *className,
                                   const char *methodName) {
  std::stringstream ss;
  ss << "No matching overload found for " << className << "::" << methodName;
  throw rt::LanguageException(
      "InternalInconsistencyException",
      RT_boxPtr(::String_createDynamicStr(ss.str().c_str())), RT_boxNil());
}

extern "C" void throwLanguageException_C(const char *name, RTValue message,
                                         RTValue payload) {
  throw rt::LanguageException(name, message, payload);
}

extern "C" void throwArityException_C(int expected, int actual) {
  std::stringstream ss;
  ss << "Wrong number of args (" << actual << ") passed to function";
  throw rt::LanguageException(
      "ArityException", RT_boxPtr(::String_createDynamicStr(ss.str().c_str())),
      RT_boxNil());
}

extern "C" void throwIllegalArgumentException_C(const char *message) {
  throw rt::LanguageException("IllegalArgumentException",
                              RT_boxPtr(::String_createDynamicStr(message)),
                              RT_boxNil());
}

extern "C" void throwIllegalStateException_C(const char *message) {
  throw rt::LanguageException("IllegalStateException",
                              RT_boxPtr(::String_createDynamicStr(message)),
                              RT_boxNil());
}

extern "C" void throwUnsupportedOperationException_C(const char *message) {
  throw rt::LanguageException("UnsupportedOperationException",
                              RT_boxPtr(::String_createDynamicStr(message)),
                              RT_boxNil());
}

extern "C" void throwArithmeticException_C(const char *message) {
  throw rt::LanguageException("ArithmeticException",
                              RT_boxPtr(::String_createDynamicStr(message)),
                              RT_boxNil());
}

extern "C" void throwIndexOutOfBoundsException_C(uword_t index, uword_t count) {
  std::stringstream ss;
  ss << "Index out of bounds: " << index << " (count: " << count << ")";
  throw rt::LanguageException(
      "IndexOutOfBoundsException",
      RT_boxPtr(::String_createDynamicStr(ss.str().c_str())), RT_boxNil());
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
      RT_boxPtr(::String_createDynamicStr(retval.str().c_str())), RT_boxNil());
}
