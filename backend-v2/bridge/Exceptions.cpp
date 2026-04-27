#include "Exceptions.h"
#include "../runtime/Exception.h"
#include "bytecode.pb.h"
#include "runtime/ObjectProto.h"
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
#include <unistd.h>

namespace rt {

struct Colors {
  static constexpr const char *RESET = "\033[0m";
  static constexpr const char *BOLD = "\033[1m";
  static constexpr const char *DIM = "\033[2m";
  static constexpr const char *RED = "\033[31m";
  static constexpr const char *GREEN = "\033[32m";
  static constexpr const char *YELLOW = "\033[33m";
  static constexpr const char *BLUE = "\033[34m";
  static constexpr const char *MAGENTA = "\033[35m";
  static constexpr const char *CYAN = "\033[36m";
  static constexpr const char *WHITE = "\033[37m";
  static constexpr const char *BRIGHT_RED = "\033[91m";
  static constexpr const char *BRIGHT_GREEN = "\033[92m";
  static constexpr const char *BRIGHT_YELLOW = "\033[93m";
  static constexpr const char *BRIGHT_BLUE = "\033[94m";
  static constexpr const char *BRIGHT_MAGENTA = "\033[95m";
  static constexpr const char *BRIGHT_CYAN = "\033[96m";
  static constexpr const char *BRIGHT_WHITE = "\033[97m";

  // Semantic colors
  static const char *HEADER(bool e) {
    return e ? "\033[1;34m" : "";
  }                                                             // Bold Blue
  static const char *MSG(bool e) { return e ? "\033[1m" : ""; } // Bold Default
  static const char *LOC(bool e) {
    return e ? "\033[1;35m" : "";
  }                                                              // Bold Magenta
  static const char *CODE(bool e) { return e ? "\033[1m" : ""; } // Bold Default
  static const char *MARK(bool e) { return e ? "\033[1;31m" : ""; } // Bold Red
  static const char *USER_FN(bool e) {
    return e ? "\033[1;32m" : "";
  }                                                               // Bold Green
  static const char *INFRA(bool e) { return e ? "\033[2m" : ""; } // Dim
};

inline const char *c(const char *code, bool enabled) {
  return enabled ? code : "";
}

thread_local std::shared_ptr<CapturedStack> gCurrentAsyncStack = nullptr;

// JitFunctionEntry and SourceLocation are now defined in Exceptions.h

static std::map<uword_t, JitFunctionEntry> gJitFunctions;
static std::mutex gJitMapMutex;
static std::mutex gSymbolizerMutex;

void registerJitFunction(
    uword_t address, uword_t size, const char *name, const void *objectData,
    size_t objectSize, std::map<rt::SourceLocation, std::string> locationToForm,
    std::map<rt::BaseSourceLocation, std::string> locationToContextForm) {
  std::lock_guard<std::mutex> lock(gJitMapMutex);
  JitFunctionEntry entry;
  entry.size = size;
  entry.name = name ? name : "unknown";
  if (objectData && objectSize > 0) {
    entry.objectData.assign(static_cast<const uint8_t *>(objectData),
                            static_cast<const uint8_t *>(objectData) +
                                objectSize);
  }
  entry.locationToForm = std::move(locationToForm);
  entry.locationToContextForm = std::move(locationToContextForm);
  gJitFunctions[address] = std::move(entry);
}

void registerJitFunction(uword_t address, uword_t size, const char *name,
                         const void *objectData, size_t objectSize) {
  registerJitFunction(address, size, name, objectData, objectSize, {}, {});
}

extern "C" void registerJitFunction_C(uword_t address, uword_t size,
                                      const char *name, const void *objectData,
                                      size_t objectSize) {
  registerJitFunction(address, size, name, objectData, objectSize, {}, {});
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
                         const intptr_t defaultSlide, StackTraceMode mode,
                         bool useColor) {
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
                      llvm::StringRef(reinterpret_cast<const char *>(
                                          entry.objectData.data()),
                                      entry.objectData.size()),
                      entry.name, false);
                  auto objOrErr = llvm::object::ObjectFile::createObjectFile(
                      buffer->getMemBufferRef());
                  if (objOrErr) {
                    uword_t offset = addr - entryStart;
                    if (offset >= 4)
                      offset -= 4;

                    // Use a LOCAL symbolizer for each JIT frame to be
                    // absolutely sure about lifetime and thread safety
                    llvm::symbolize::LLVMSymbolizer localSymbolizer;
                    auto resOrErr = localSymbolizer.symbolizeInlinedCode(
                        *objOrErr.get(),
                        {offset, llvm::object::SectionedAddress::UndefSection});

                    if (resOrErr) {
                      auto &inlinedInfo = resOrErr.get();
                      for (uint32_t i = 0; i < inlinedInfo.getNumberOfFrames();
                           ++i) {
                        auto &info = inlinedInfo.getFrame(i);
                        std::string fnName =
                            (info.FunctionName != "<invalid>")
                                ? llvm::demangle(info.FunctionName)
                                : demangled;

                        std::stringstream frameSs;
                        frameSs << "  " << Colors::INFRA(useColor) << "at "
                                << c(Colors::RESET, useColor)
                                << Colors::USER_FN(useColor) << fnName
                                << c(Colors::RESET, useColor);
                        if (info.FileName != "<invalid>")
                          frameSs << " [" << Colors::LOC(useColor)
                                  << info.FileName << ":" << info.Line
                                  << c(Colors::RESET, useColor) << "]";
                        else
                          frameSs << " [JIT:0x" << std::hex << entryStart
                                  << std::dec << "]";

                        if (mode == StackTraceMode::Debug && i > 0)
                          frameSs << " (inlined)";
                        if (mode == StackTraceMode::Debug)
                          frameSs << " @ 0x" << std::hex << addr << std::dec;

                        frameSs << "\n";

                        // NEW: If this is the first user frame or we're looking
                        // for the form, check the locationToForm map
                        if (!entry.locationToForm.empty() &&
                            info.FileName != "<invalid>") {
                          SourceLocation loc;
                          loc.file = info.FileName;
                          loc.line = info.Line;
                          loc.column = info.Column;
                          auto formIt = entry.locationToForm.find(loc);
                          if (formIt != entry.locationToForm.end()) {
                            // This is a candidate for the exception's form
                            // We'll handle this in the constructor's eager
                            // symbolization
                          }
                        }

                        // Deduplicate with previous lines
                        if (currentAddrLines.empty() ||
                            currentAddrLines.back() != frameSs.str()) {
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
                  frameSs << "  " << c(Colors::DIM, useColor) << "at "
                          << c(Colors::RESET, useColor)
                          << c(Colors::BRIGHT_GREEN, useColor)
                          << c(Colors::BOLD, useColor) << demangled
                          << c(Colors::RESET, useColor) << " [JIT:0x"
                          << std::hex << entryStart << std::dec << "]";
                  if (mode == StackTraceMode::Debug) {
                    frameSs << " [+0x" << std::hex << (addr - entryStart)
                            << std::dec << "] @ 0x" << std::hex << addr
                            << std::dec;
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
                  frameSs << "  " << Colors::INFRA(useColor) << "at "
                          << c(Colors::RESET, useColor)
                          << Colors::USER_FN(useColor) << demangled
                          << c(Colors::RESET, useColor);
                  if (info.FileName != "<invalid>")
                    frameSs << " [" << Colors::LOC(useColor) << info.FileName
                            << ":" << info.Line << c(Colors::RESET, useColor)
                            << "]";
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
            if ((foundFirstUserFrame &&
                 (mode == StackTraceMode::Debug || !isInfra)) ||
                mode == StackTraceMode::Debug) {
              std::stringstream frameSs;
              if (!dlSymbolName.empty()) {
                frameSs << "  " << Colors::INFRA(useColor) << "at "
                        << c(Colors::RESET, useColor) << Colors::INFRA(useColor)
                        << demangled << c(Colors::RESET, useColor) << " ["
                        << (dlinfo.dli_fname ? dlinfo.dli_fname : "unknown")
                        << "]";
              } else {
                frameSs << "  " << Colors::INFRA(useColor) << "at "
                        << c(Colors::RESET, useColor) << "0x" << std::hex
                        << addr << std::dec << " ["
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

  // Eager symbolization for form lookup
#ifdef __APPLE__
  char path[1024];
  uint32_t _size = sizeof(path);
  _NSGetExecutablePath(path, &_size);
#endif

  bool foundForm = false;
  for (uword_t addr : capturedStack->addresses) {
    if (addr == 0)
      continue;

    // Check JIT functions for form
    if (!foundForm) {
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
          if (!entry.objectData.empty()) {
            auto buffer = llvm::MemoryBuffer::getMemBuffer(
                llvm::StringRef(
                    reinterpret_cast<const char *>(entry.objectData.data()),
                    entry.objectData.size()),
                entry.name, false);
            auto objOrErr = llvm::object::ObjectFile::createObjectFile(
                buffer->getMemBufferRef());
            if (objOrErr) {
              uword_t offset = addr - entryStart;
              if (offset >= 4)
                offset -= 4;

              llvm::symbolize::LLVMSymbolizer localSymbolizer;
              auto resOrErr = localSymbolizer.symbolizeInlinedCode(
                  *objOrErr.get(),
                  {offset, llvm::object::SectionedAddress::UndefSection});

              if (resOrErr) {
                auto &inlinedInfo = resOrErr.get();
                for (uint32_t i = 0; i < inlinedInfo.getNumberOfFrames(); ++i) {
                  auto &info = inlinedInfo.getFrame(i);
                  if (info.FileName != "<invalid>") {
                    SourceLocation exactLoc(info.FileName, info.Line, info.Column, info.Discriminator);
                    BaseSourceLocation baseLoc(exactLoc);
                    // Try to find exact form
                    auto exactIt = entry.locationToForm.find(exactLoc);
                    if (exactIt != entry.locationToForm.end()) {
                      this->exactForm = exactIt->second;
                    }

                    // Try to find context form
                    auto contextIt = entry.locationToContextForm.find(baseLoc);
                    if (contextIt != entry.locationToContextForm.end()) {
                      this->form = contextIt->second;
                    }

                    // If either is missing, try fuzzy matching for BOTH
                    if (this->exactForm.empty() || this->form.empty()) {
                      std::string base = info.FileName;
                      size_t lastS = base.find_last_of('/');
                      if (lastS != std::string::npos)
                        base = base.substr(lastS + 1);

                      for (auto const &[k, v] : entry.locationToForm) {
                        std::string kBase = k.file;
                        size_t kLastS = kBase.find_last_of('/');
                        if (kLastS != std::string::npos)
                          kBase = kBase.substr(kLastS + 1);

                        if (kBase == base && k.line == (int)info.Line) {
                          if (this->exactForm.empty() &&
                              k.column == (int)info.Column &&
                              k.discriminator == (int)info.Discriminator) {
                            this->exactForm = v;
                          }
                        }
                      }
                      
                      for (auto const &[k, v] : entry.locationToContextForm) {
                        std::string kBase = k.file;
                        size_t kLastS = kBase.find_last_of('/');
                        if (kLastS != std::string::npos)
                          kBase = kBase.substr(kLastS + 1);

                        if (kBase == base && k.line == (int)info.Line && k.column == (int)info.Column) {
                           if (this->form.empty()) {
                             this->form = v;
                           }
                        }
                      }
                    }

                    if (!this->form.empty() || !this->exactForm.empty()) {
                      std::stringstream locSs;
                      locSs << info.FileName << ":" << info.Line << ":"
                            << info.Column;
                      if (info.Discriminator > 0)
                        locSs << " (disc " << info.Discriminator << ")";
                      this->sourceLocation = locSs.str();
                      foundForm = true;
                      break;
                    }
                  }
                }
              } else {
                llvm::consumeError(resOrErr.takeError());
              }
            } else {
              llvm::consumeError(objOrErr.takeError());
            }
          }
        }
      }
    }
    if (foundForm)
      break;
  }
}

LanguageException::LanguageException(const std::string &name, RTValue message,
                                     RTValue payload, const std::string &form,
                                     const std::string &exactForm,
                                     const std::string &sourceLocation)
    : name(name), message(message), payload(payload), form(form),
      exactForm(exactForm), sourceLocation(sourceLocation) {
  capturedStack = captureCurrentStack();
}

LanguageException::LanguageException(const LanguageException &other)
    : name(other.name), message(other.message), payload(other.payload),
      form(other.form), sourceLocation(other.sourceLocation),
      capturedStack(other.capturedStack) {
  retain(message);
  retain(payload);
  // cachedMessage is empty, messageMutex is default initialized
}

LanguageException &
LanguageException::operator=(const LanguageException &other) {
  if (this != &other) {
    std::lock_guard<std::mutex> lock(messageMutex);
    release(message);
    release(payload);
    name = other.name;
    message = other.message;
    payload = other.payload;
    form = other.form;
    sourceLocation = other.sourceLocation;
    capturedStack = other.capturedStack;
    cachedMessage = "";
    retain(message);
    retain(payload);
  }
  return *this;
}

LanguageException::~LanguageException() noexcept {
  release(message);
  release(payload);
}

const char *LanguageException::what() const noexcept {
  std::lock_guard<std::mutex> lock(messageMutex);
  if (cachedMessage.empty()) {
    cachedMessage = getExceptionString(*this, StackTraceMode::Debug, false);
  }
  return cachedMessage.c_str();
}

std::string
LanguageException::toString(llvm::symbolize::LLVMSymbolizer &symbolizer,
                            const std::string &moduleName, const intptr_t slide,
                            StackTraceMode mode, bool useColor) const {
  std::stringstream ss;

  if (useColor) {
    const char *force = getenv("CLOJURE_RT_FORCE_COLOR");
    if (force && std::string(force) == "1") {
      useColor = true;
    } else {
      useColor = isatty(STDERR_FILENO);
    }
  }

  ss << "\n " << Colors::MARK(useColor) << "[!] " << c(Colors::RESET, useColor)
     << Colors::HEADER(useColor) << "Exception: " << name
     << c(Colors::RESET, useColor) << "\n";

  retain(message);
  String *msgStr = String_compactify(::toString(message));
  ss << "     " << Colors::MSG(useColor) << "Message: " << String_c_str(msgStr)
     << c(Colors::RESET, useColor) << "\n";
  Ptr_release(msgStr);

  retain(payload);
  if (payload != 0) {
    String *payStr = String_compactify(::toString(payload));
    ss << "     " << c(Colors::WHITE, useColor)
       << "Payload: " << String_c_str(payStr) << c(Colors::RESET, useColor)
       << "\n";
    Ptr_release(payStr);
  }

  if (!form.empty()) {
    ss << "\n " << c(Colors::BOLD, useColor)
       << "Source context:" << c(Colors::RESET, useColor) << "\n";
    ss << "  " << Colors::LOC(useColor) << sourceLocation
       << c(Colors::RESET, useColor) << ":\n";
    ss << "    " << Colors::CODE(useColor) << form << c(Colors::RESET, useColor)
       << "\n";
    ss << "    " << Colors::MARK(useColor) << std::string(form.length(), '^')
       << c(Colors::RESET, useColor) << "\n";
  }

  if (!exactForm.empty() && exactForm != form) {
    ss << "\n " << c(Colors::BOLD, useColor)
       << "Exact expression:" << c(Colors::RESET, useColor) << "\n";
    ss << "    " << Colors::CODE(useColor) << exactForm
       << c(Colors::RESET, useColor) << "\n";
  }

  ss << "\n " << c(Colors::BOLD, useColor)
     << "Stack Trace:" << c(Colors::RESET, useColor) << "\n";
  symbolizeStackChain(ss, capturedStack, symbolizer, moduleName, slide, mode,
                      useColor);
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

std::string getExceptionString(const LanguageException &e, StackTraceMode mode,
                               bool useColor) {
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

  return e.toString(symbolizer, moduleName, slide, mode, useColor);
}

void throwInternalInconsistencyException(const std::string &errorMessage) {
  throw rt::LanguageException(
      "InternalInconsistencyException",
      RT_boxPtr(::String_createDynamicStr(errorMessage.c_str())), RT_boxNil());
}

extern "C" void throwInternalInconsistencyException_C(String *errorMessage) {
  throw rt::LanguageException("InternalInconsistencyException",
                              RT_boxPtr(errorMessage), RT_boxNil());
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

extern "C" [[noreturn]] void
throwNoMatchingConstructorException_C(const char *className,
                                       int arity) {
  std::stringstream ss;
  ss << "No matching constructor found for " << className << " with arity " << arity;
  throw rt::LanguageException(
      "NoMatchingConstructorException",
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
                                  const std::string &form,
                                  const std::string &file, int line,
                                  int column) {
  std::stringstream retval;
  retval << file << ":" << line << ":" << column << ": error: " << errorMessage
         << "\n";
  retval << form << "\n";
  retval << std::string(form.length(), '^') << "\n";

  std::stringstream locSs;
  locSs << file << ":" << line << ":" << column;

  throw rt::LanguageException(
      "CodeGenerationException",
      RT_boxPtr(::String_createDynamicStr(retval.str().c_str())), RT_boxNil(),
      form, form, locSs.str());
}

void throwCodeGenerationException(
    const std::string &errorMessage,
    const clojure::rt::protobuf::bytecode::Node &node) {
  std::string file = "unknown";
  int line = 0;
  int column = 0;
  if (node.has_env()) {
    file = node.env().file();
    line = node.env().line();
    column = node.env().column();
  }
  throwCodeGenerationException(errorMessage, node.form(), file, line, column);
}

extern "C" String *exceptionToString_C(void *exception) {
  rt::LanguageException *ex = (rt::LanguageException *)exception;
  auto str = getExceptionString(*ex, StackTraceMode::Friendly, true);
  return ::String_createDynamicStr(str.c_str());
}

extern "C" void *createException_C(const char *className, String *message,
                                   RTValue payload) {
  return new rt::LanguageException(className, RT_boxPtr(message), payload);
}

extern "C" [[noreturn]] void throwException_C(RTValue exceptionBoxed) {
  if (getType(exceptionBoxed) != exceptionType) {
    release(exceptionBoxed);
    throwInternalInconsistencyException(
        "throwException_C called with non-exception object");
  }
  Exception *ex = (Exception *)RT_unboxPtr(exceptionBoxed);
  rt::LanguageException *le = (rt::LanguageException *)ex->bridgedData;
  rt::LanguageException toThrow = *le;
  release(exceptionBoxed);
  throw toThrow;
}

extern "C" void deleteException_C(void *exception) {
  delete (rt::LanguageException *)exception;
}

} // namespace rt
