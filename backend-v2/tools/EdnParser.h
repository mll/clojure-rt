#ifndef EDN_PARSER_H
#define EDN_PARSER_H

#include "../RuntimeHeaders.h"
#include "../bridge/Exceptions.h"
#include "../types/ObjectTypeSet.h"
#include <memory>
#include <unordered_map>
using namespace std;

struct Class;

namespace rt {

class TemporaryClassData {
  void scanMetadata(RTValue from);

public:
  unordered_map<string, PersistentArrayMap *> classesByName;
  unordered_map<uword_t, PersistentArrayMap *> classesById;
  uint32_t nextClassId = 1000;
  TemporaryClassData(RTValue from);
  ~TemporaryClassData();
  void clear();
};

enum class CallType { Call, Intrinsic, ClojureFn };

class IntrinsicDescription {
public:
  vector<ObjectTypeSet> argTypes;
  CallType type;
  string symbol;
  ObjectTypeSet returnType;
  ObjectTypeSet thisType;
  bool isInstance;
  bool returnsProvided;
  IntrinsicDescription() = default;
  IntrinsicDescription(
      RTValue from, TemporaryClassData &classData,
      const ObjectTypeSet &thisType = ObjectTypeSet::all(),
      bool isInstance = false,
      const ObjectTypeSet &defaultReturnType = ObjectTypeSet::dynamicType());
  std::string toString() const;
};

class ClassDescription {
  unordered_map<string, RTValue> parseStaticFields(RTValue from);
  unordered_map<string, vector<IntrinsicDescription>>
  parseIntrinsics(RTValue from, TemporaryClassData &classData,
                  const ObjectTypeSet &thisType = ObjectTypeSet::all(),
                  bool isInstance = false);
  vector<IntrinsicDescription>
  parseConstructorDescriptions(RTValue from, TemporaryClassData &classData,
                               const ObjectTypeSet &thisType);

public:
  ObjectTypeSet type;
  ::Class *extends;
  string name;
  string parentName;
  unordered_map<string, RTValue> staticFields;
  vector<RTValue> staticFieldValues;
  unordered_map<string, int32_t> staticFieldIndices;

  unordered_map<string, int32_t> instanceFields;

  vector<IntrinsicDescription> constructors;
  unordered_map<string, vector<IntrinsicDescription>> staticFns;
  unordered_map<string, vector<IntrinsicDescription>> instanceFns;

  vector<Class *> interfaces;

  ClassDescription() = default;
  ClassDescription(RTValue from, TemporaryClassData &classData);
  ClassDescription(const ClassDescription &) = delete;
  ClassDescription &operator=(const ClassDescription &) = delete;
  ClassDescription(ClassDescription &&other) noexcept = default;
  ClassDescription &operator=(ClassDescription &&other) noexcept = default;
  ~ClassDescription();
  std::string toString() const;
};

extern "C" {
void delete_class_description(void *p);
int32_t ClassExtension_fieldIndex(void *ext, RTValue field);
int32_t ClassExtension_staticFieldIndex(void *ext, RTValue staticField);
RTValue ClassExtension_getIndexedStaticField(void *ext, int32_t i);
RTValue ClassExtension_setIndexedStaticField(void *ext, int32_t i,
                                             RTValue value);
ClojureFunction *ClassExtension_resolveInstanceCall(void *ext, RTValue name,
                                                    int32_t argCount);
}

vector<unique_ptr<ClassDescription>> buildClasses(RTValue from);
} // namespace rt

#endif
