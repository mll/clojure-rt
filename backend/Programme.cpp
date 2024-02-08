#include "static/Numbers.h"
#include "static/Utils.h"
#include "static/Vector.h"
#include "Programme.h"

ProgrammeState::ProgrammeState() {
  auto numbers = getNumbersStaticFunctions();
  auto utils = getUtilsStaticFunctions();
  auto vector = getVectorStaticAndInstanceFunctions();
  StaticCallLibrary.insert(numbers.begin(), numbers.end());
  StaticCallLibrary.insert(utils.begin(), utils.end());
  StaticCallLibrary.insert(vector.first.begin(), vector.first.end());
  InstanceCallLibrary.insert(vector.second.begin(), vector.second.end());
}
