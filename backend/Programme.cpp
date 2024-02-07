#include "static/Numbers.h"
#include "static/Utils.h"
#include "Programme.h"


std::string ProgrammeState::closedOverKey(uint64_t functionId, uint64_t methodId) {
  return std::to_string(functionId)+ "_" + std::to_string(methodId);
}


ProgrammeState::ProgrammeState() {
  auto numbers = getNumbersStaticFunctions();
  auto utils = getUtilsStaticFunctions();
  StaticCallLibrary.insert(numbers.begin(), numbers.end());
  StaticCallLibrary.insert(utils.begin(), utils.end());
}
