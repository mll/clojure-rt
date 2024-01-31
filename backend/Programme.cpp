#include "static/Numbers.h"
#include "static/Utils.h"
#include "static/Vector.h"
#include "Programme.h"

ProgrammeState::ProgrammeState() {
  auto numbers = getNumbersStaticFunctions();
  auto utils = getUtilsStaticFunctions();
  auto vector = getVectorStaticFunctions();
  StaticCallLibrary.insert(numbers.begin(), numbers.end());
  StaticCallLibrary.insert(utils.begin(), utils.end());
  StaticCallLibrary.insert(vector.begin(), vector.end());
}
