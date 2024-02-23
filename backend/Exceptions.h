#ifndef RT_EXCEPTIONS
#define RT_EXCEPTIONS

#include "bytecode.pb.h"

using namespace clojure::rt::protobuf::bytecode;

class CodeGenerationException: public std::exception {
  std::string errorMessage;
  public:
  CodeGenerationException(const std::string &errorMessage, const Node& node); 
  std::string toString() const; 
};

class InternalInconsistencyException: public std::exception {
  std::string errorMessage;
  public:
  InternalInconsistencyException(const std::string &error) : errorMessage(error) {} 
  std::string toString() const { return errorMessage; } 
};

#endif
