#include "static/Numbers.h"
#include "static/Utils.h"
#include "Programme.h"

ProgrammeState::ProgrammeState() {
  auto numbers = getNumbersStaticFunctions();
  auto utils = getUtilsStaticFunctions();
  StaticCallLibrary.insert(numbers.begin(), numbers.end());
  StaticCallLibrary.insert(utils.begin(), utils.end());
}
