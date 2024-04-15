#include "static/Class.h"
#include "static/Numbers.h"
#include "static/Utils.h"
#include "static/Vector.h"
#include "Programme.h"

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
  StaticCallLibrary.insert(numbers.begin(), numbers.end());
  StaticCallLibrary.insert(utils.begin(), utils.end());
  StaticCallLibrary.insert(vector.first.begin(), vector.first.end());
  DynamicCallLibrary.insert(vector.second.first.begin(), vector.second.first.end());
  InstanceCallLibrary.insert(vector.second.second.begin(), vector.second.second.end());
  
  // C++ insert semantics: if key is already present in map, insert will be ignored
  for (uint64_t t = integerType; t <= persistentArrayMapType; ++t) {
    DynamicCallLibrary.insert({t, {}});
    InstanceCallLibrary.insert({t, {}});
  }
  
  uint64_t javaLangClassId = registerClass(javaLangClass());
}
