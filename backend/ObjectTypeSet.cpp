#include "ObjectTypeSet.h"

bool operator==(const ObjectTypeSet &first, const ObjectTypeSet &second) {
    return first.internal == second.internal && first.isBoxed == second.isBoxed;
}

bool operator<(const ObjectTypeSet &first, const ObjectTypeSet &second) {
    return first.toString() < second.toString();
}
