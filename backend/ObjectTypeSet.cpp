#include "ObjectTypeSet.h"

bool operator==(const ObjectTypeSet &first, const ObjectTypeSet &second) {
    return first.internal == second.internal;
}

bool operator<(const ObjectTypeSet &first, const ObjectTypeSet &second) {
    return first.toString() < second.toString();
}
