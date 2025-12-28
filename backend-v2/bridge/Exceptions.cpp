#include "Exceptions.h"
#include <stdio.h>
#include <iomanip>
#include <sys/_types/_intptr_t.h>

namespace rt {
  
  LanguageException::LanguageException(const std::string &name, RTValue message, RTValue payload) {
    this->payload = payload;
    this->name = name;
    this->message = message;
    void* buffer[128];
    int size = backtrace(buffer, 128);
    stackAddresses.reserve(size);
    for(int i = 0; i < size; i++) {
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

    for (uword_t addr : stackAddresses) {

      uword_t lookupAddr = addr - slide;      
      
      auto resOrErr = symbolizer.symbolizeCode(moduleName, {lookupAddr, llvm::object::SectionedAddress::UndefSection});
      
      ss << "  at ";
      if (resOrErr) {
        auto& info = resOrErr.get();
        if (info.FunctionName != "<invalid>") {
          ss << info.FunctionName;
        } else {
          ss << "0x" << std::hex << std::setw(sizeof(uword_t) * 2) 
             << std::setfill('0') << addr << std::dec;
        }
        if (info.FileName != "<invalid>") {
          ss << " [" << info.FileName << ":" << info.Line << "]";
        }
      } else {
        ss << "?? [0x" << std::hex << addr << std::dec << "]";
        llvm::consumeError(resOrErr.takeError());
      }
      ss << "\n";
    }
    
    return ss.str();
  }
}


extern "C" {
  void throwInternalInconsistencyException(const std::string &errorMessage) {
    throw rt::LanguageException("InternalInconsistencyException", RT_boxPtr(String_createDynamicStr(errorMessage.c_str())), RT_boxNil());
  }
  
  void throwCodeGenerationException(const std::string &errorMessage,
                                    const Node &node) {
    std::stringstream retval;
    auto env = node.env();
    retval << env.file() <<  ":" << env.line() << ":" << env.column() << ": error: " << errorMessage << "\n";
    retval << node.form() << "\n";
    retval << std::string(node.form().length(), '^') << "\n";
    throw rt::LanguageException("CodeGenerationException", RT_boxPtr(String_createDynamicStr(retval.str().c_str())), RT_boxNil());
  }
}

