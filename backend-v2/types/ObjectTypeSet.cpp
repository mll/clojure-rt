#include "ObjectTypeSet.h"

namespace rt {

bool operator==(const ObjectTypeSet &first, const ObjectTypeSet &second) {
  return first.internal == second.internal && first.isBoxed == second.isBoxed &&
         first.allTypes == second.allTypes;
}

bool operator<(const ObjectTypeSet &first, const ObjectTypeSet &second) {
  return first.toString() < second.toString();
}

} // namespace rt
