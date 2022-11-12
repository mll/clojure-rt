#ifndef RT_EXCEPTIONS
#define RT_EXCEPTIONS

#include "protobuf/bytecode.pb.h"

using namespace clojure::rt::protobuf::bytecode;

class CodeGenerationException: public exception {
  string errorMessage;
  public:
  CodeGenerationException(const string &errorMessage, const Node& node); 
  string toString() const; 
};

class InternalInconsistencyException: public exception {
  string errorMessage;
  public:
  InternalInconsistencyException(const string &error) : errorMessage(error) {} 
  string toString() const { return errorMessage; } 
};

class UnaccountedRecursiveFunctionEncounteredException: public exception {
  public:
  string functionName;
  UnaccountedRecursiveFunctionEncounteredException(const string &functionName): functionName(functionName) { }
};

#endif
