#include "static/Class.h"
#include "static/Numbers.h"
#include "static/Utils.h"
#include "static/Vector.h"
#include "static/Deftype.h"
#include "Programme.h"

extern "C" {
  #include "runtime/Deftype.h"
  void release(void *obj);
  void retain(void *obj);
}

uint64_t ProgrammeState::getUniqueClassId() {
  /* TODO - this might require threadsafe precautions, like std::Atomic */
  return ++lastClassUniqueId;
}

uint64_t ProgrammeState::registerClass(Class *_class) {
  std::string string_className {String_c_str(_class->className)};
  auto classId = getUniqueClassId();
  _class->registerId = classId;
  auto it = ClassesByName.find(string_className); // Overwrite older class with the same name if present
  if (it != ClassesByName.end()) {
    it->second = classId;
  } else {
    ClassesByName.insert({string_className, classId});
  }
  DefinedClasses.insert({classId, _class});
  DynamicCallLibrary.insert({classId, {}});
  InstanceCallLibrary.insert({classId, {}});
  return classId;
}

std::string ProgrammeState::closedOverKey(uint64_t functionId, uint64_t methodId) {
  return std::to_string(functionId)+ "_" + std::to_string(methodId);
}


ProgrammeState::ProgrammeState() {
  auto numbers = getNumbersStaticFunctions();
  auto utils = getUtilsStaticFunctions();
  auto vector = getVectorFunctions();
  auto deftype = getDeftypeStaticFunctions();
  StaticCallLibrary.insert(numbers.begin(), numbers.end());
  StaticCallLibrary.insert(utils.begin(), utils.end());
  StaticCallLibrary.insert(vector.first.begin(), vector.first.end());
  DynamicCallLibrary.insert(vector.second.first.begin(), vector.second.first.end());
  InstanceCallLibrary.insert(vector.second.second.begin(), vector.second.second.end());
  DynamicCallLibrary.insert(deftype.first.begin(), deftype.first.end());
  InstanceCallLibrary.insert(deftype.second.begin(), deftype.second.end());
  
  // C++ insert semantics: if key is already present in map, insert will be ignored
  for (uint64_t t = integerType; t <= persistentArrayMapType; ++t) {
    DynamicCallLibrary.insert({t, {}});
    InstanceCallLibrary.insert({t, {}});
  }
  
  uint64_t javaLangClassId = registerClass(javaLangClass());
  uint64_t javaLangLongId = registerClass(javaLangLong());
}

uint64_t ProgrammeState::getClassId(const std::string &className) {
  auto foundClass = ClassesByName.find(className);
  if (foundClass == ClassesByName.end()) return 0; // CONSIDER: exception?
  return foundClass->second;
}

Class *ProgrammeState::getClass(uint64_t classId) {
  auto foundClass = DefinedClasses.find(classId);
  if (foundClass == DefinedClasses.end()) return nullptr;
  retain(foundClass->second);
  return foundClass->second;
}

void *ProgrammeState::getPrimitiveMethod(objectType target, const std::string &methodName, const std::vector<objectType> &argTypes) {
  auto classMethods = DynamicCallLibrary.find((uint64_t) target);
  if (classMethods == DynamicCallLibrary.end()) return nullptr;
  
  std::vector<ObjectTypeSet> types {target};
  types.insert(types.end(), argTypes.begin(), argTypes.end());
  std::string requiredTypes = ObjectTypeSet::typeStringForArgs(types);
  
  auto methods = classMethods->second.find(methodName);
  if (methods != classMethods->second.end()) { // class and method found
    for (auto method: methods->second) {
      if (method.first != requiredTypes) continue; 
      return method.second;
    }
  }
  
  return nullptr;
}
