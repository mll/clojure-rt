#include "Exceptions.h"
#include "llvm/DebugInfo/Symbolize/Symbolize.h"
#include "llvm/Demangle/Demangle.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/MemoryBuffer.h"
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
  std::vector<uint8_t> objectData;
};

static std::map<uword_t, JitFunctionEntry> gJitFunctions;
static std::mutex gJitMapMutex;
static std::mutex gSymbolizerMutex;

void registerJitFunction(uword_t address, uword_t size, const char *name,
                         const void *objectData, size_t objectSize) {
  std::lock_guard<std::mutex> lock(gJitMapMutex);
  JitFunctionEntry entry;
  entry.size = size;
  entry.name = name ? name : "unknown";
  if (objectData && objectSize > 0) {
    entry.objectData.assign(static_cast<const uint8_t *>(objectData),
                             static_cast<const uint8_t *>(objectData) + objectSize);
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
      if (addr == 0)
        continue;
      bool addrHandled = false;
      std::vector<std::string> currentAddrLines;

      // 1. Check JIT functions
      {
        std::lock_guard<std::mutex> lock(gJitMapMutex);
        auto it = gJitFunctions.lower_bound(addr);
        if (it != gJitFunctions.begin() &&
            (it == gJitFunctions.end() || it->first > addr)) {
          --it;
        }

        if (it != gJitFunctions.end()) {
          uword_t entryStart = it->first;
          const JitFunctionEntry &entry = it->second;
          if (addr >= entryStart && addr < entryStart + entry.size) {
            std::string demangled = llvm::demangle(entry.name);
            bool isInfra = isInfrastructureFrame(demangled, "");
            
            if (!foundFirstUserFrame && !isInfra)
              foundFirstUserFrame = true;

            if (foundFirstUserFrame || mode == StackTraceMode::Debug) {
              if (mode == StackTraceMode::Debug || !isInfra) {
                // Try to get detailed info from objectData if present
                bool detailedInfoFound = false;
                if (!entry.objectData.empty()) {
                  auto buffer = llvm::MemoryBuffer::getMemBuffer(
                      llvm::StringRef(reinterpret_cast<const char*>(entry.objectData.data()), 
                                    entry.objectData.size()),
                      entry.name, false);
                  auto objOrErr = llvm::object::ObjectFile::createObjectFile(buffer->getMemBufferRef());
                  if (objOrErr) {
                    uword_t offset = addr - entryStart;
                    if (offset >= 4) offset -= 4;
                    
                    // Use a LOCAL symbolizer for each JIT frame to be absolutely sure about lifetime and thread safety
                    llvm::symbolize::LLVMSymbolizer localSymbolizer;
                    auto resOrErr = localSymbolizer.symbolizeInlinedCode(
                        *objOrErr.get(), 
                        {offset, llvm::object::SectionedAddress::UndefSection});
                    
                    if (resOrErr) {
                      auto &inlinedInfo = resOrErr.get();
                      for (uint32_t i = 0; i < inlinedInfo.getNumberOfFrames(); ++i) {
                        auto &info = inlinedInfo.getFrame(i);
                        std::string fnName = (info.FunctionName != "<invalid>") 
                                               ? llvm::demangle(info.FunctionName) 
                                               : demangled;
                        
                        std::stringstream frameSs;
                        frameSs << "  at " << fnName;
                        if (info.FileName != "<invalid>")
                          frameSs << " [" << info.FileName << ":" << info.Line << "]";
                        else
                          frameSs << " [JIT:0x" << std::hex << entryStart << std::dec << "]";
                        
                        if (mode == StackTraceMode::Debug && i > 0)
                          frameSs << " (inlined)";
                        if (mode == StackTraceMode::Debug)
                          frameSs << " @ 0x" << std::hex << addr << std::dec;
                        
                        frameSs << "\n";
                        
                        // Deduplicate with previous lines
                        if (currentAddrLines.empty() || currentAddrLines.back() != frameSs.str()) {
                          currentAddrLines.push_back(frameSs.str());
                        }
                        detailedInfoFound = true;
                      }
                    } else {
                      llvm::consumeError(resOrErr.takeError());
                    }
                  } else {
                    llvm::consumeError(objOrErr.takeError());
                  }
                }

                if (!detailedInfoFound) {
                  std::stringstream frameSs;
                  frameSs << "  at " << demangled << " [JIT:0x" << std::hex
                          << entryStart << std::dec << "]";
                  if (mode == StackTraceMode::Debug) {
                    frameSs << " [+0x" << std::hex << (addr - entryStart)
                            << std::dec << "] @ 0x" << std::hex << addr << std::dec;
                  }
                  frameSs << "\n";
                  currentAddrLines.push_back(frameSs.str());
                }
              }
            }
            addrHandled = true;
          }
        }
      }

      // 2. Check non-JIT symbols
      if (!addrHandled) {
        Dl_info dlinfo;
        if (dladdr(reinterpret_cast<void *>(addr), &dlinfo)) {
          std::string dlSymbolName = dlinfo.dli_sname ? dlinfo.dli_sname : "";
          std::string demangledDl = llvm::demangle(dlSymbolName);

          // Skip thread entry points if we have a parent to stitch to
          if (current->parent &&
              (dlSymbolName == "thread_start" ||
               dlSymbolName == "_pthread_start" || dlSymbolName == "start" ||
               dlSymbolName == "asan_thread_start")) {
            continue;
          }

          std::string currentModule = "";
          uword_t symbolizeAddr = 0;

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

          if (!currentModule.empty()) {
            if (symbolizeAddr >= 4)
              symbolizeAddr -= 4;

            std::lock_guard<std::mutex> sLock(gSymbolizerMutex);
            auto resOrErr = symbolizer.symbolizeInlinedCode(
                currentModule,
                {symbolizeAddr, llvm::object::SectionedAddress::UndefSection});

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
                  else
                    frameSs << " [" << currentModule << "]";
                  frameSs << "\n";
                  currentAddrLines.push_back(frameSs.str());
                } else if (mode == StackTraceMode::Debug) {
                  std::stringstream frameSs;
                  frameSs << "  at " << demangled;
                  if (info.FileName != "<invalid>")
                    frameSs << " [" << info.FileName << ":" << info.Line << "]";
                  else
                    frameSs << " [" << currentModule << "]";
                  if (i > 0)
                    frameSs << " (inlined)";
                  frameSs << " @ 0x" << std::hex << addr << std::dec;
                  frameSs << "\n";
                  currentAddrLines.push_back(frameSs.str());
                }
              }
            } else {
              llvm::consumeError(resOrErr.takeError());
            }
          }

          if (currentAddrLines.empty()) {
            std::string demangled = llvm::demangle(dlSymbolName);
            bool isInfra = isInfrastructureFrame(demangled, "");
            if (!foundFirstUserFrame && !isInfra)
              foundFirstUserFrame = true;
            if ((foundFirstUserFrame && (mode == StackTraceMode::Debug || !isInfra)) || mode == StackTraceMode::Debug) {
              std::stringstream frameSs;
              if (!dlSymbolName.empty()) {
                frameSs << "  at " << demangled << " ["
                        << (dlinfo.dli_fname ? dlinfo.dli_fname : "unknown")
                        << "]";
              } else {
                frameSs << "  at 0x" << std::hex << addr << std::dec << " ["
                        << (dlinfo.dli_fname ? dlinfo.dli_fname : "unknown")
                        << "]";
              }
              frameSs << "\n";
              currentAddrLines.push_back(frameSs.str());
            }
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

  retain(message);
  String *msgStr = String_compactify(::toString(message));
  ss << "Message: " << String_c_str(msgStr) << "\n";
  Ptr_release(msgStr);

  retain(payload);
  String *payStr = String_compactify(::toString(payload));
  ss << "Payload: " << (payload == 0 ? "nil" : String_c_str(payStr)) << "\n";
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
      "NoMatchingOverloadException",
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
