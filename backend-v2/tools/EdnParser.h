#ifndef EDN_PARSER_H
#define EDN_PARSER_H

#include "../RuntimeHeaders.h"
#include "../bridge/Exceptions.h"
#include "../types/ObjectTypeSet.h"
#include <unordered_map>
using namespace std;

namespace rt {

class TemporaryClassData {
public:
  unordered_map<string, PersistentArrayMap *> classesByName;
  unordered_map<uword_t, PersistentArrayMap *> classesById;
  TemporaryClassData(RTValue from);
  ~TemporaryClassData();
};

enum class CallType { Call, Intrinsic };

class IntrinsicDescription {
public:
  vector<ObjectTypeSet> argTypes;
  CallType type;
  string symbol;
  ObjectTypeSet returnType;
  IntrinsicDescription(RTValue from, TemporaryClassData &classData);
};

class ClassDescription {
  unordered_map<string, RTValue> parseStaticFields(RTValue from);
  unordered_map<string, vector<IntrinsicDescription>>
  parseIntrinsics(RTValue from, TemporaryClassData &classData);

public:
  ObjectTypeSet type;
  ClassDescription *extends;
  string name;
  unordered_map<string, RTValue> staticFields;
  unordered_map<string, vector<IntrinsicDescription>> staticFns;
  unordered_map<string, vector<IntrinsicDescription>> instanceFns;

  ClassDescription(RTValue from, TemporaryClassData &classData);
  ClassDescription(ClassDescription &&other) = default;
  ClassDescription &operator=(ClassDescription &&other) = default;
  ~ClassDescription();
};

vector<ClassDescription> buildClasses(RTValue from);
} // namespace rt

#endif
