#include "static/Class.h"
#include "static/Numbers.h"
#include "static/Utils.h"
#include "static/Vector.h"
#include "Programme.h"


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
}
