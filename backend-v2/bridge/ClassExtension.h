#ifndef BRIDGE_CLASS_EXTENSION_H
#define BRIDGE_CLASS_EXTENSION_H

#include "../RuntimeHeaders.h"
#include <cstdint>

extern "C" {
int32_t ClassExtension_fieldIndex(void *ext, RTValue field);
int32_t ClassExtension_staticFieldIndex(void *ext, RTValue staticField);
RTValue ClassExtension_getIndexedStaticField(void *ext, int32_t i);
RTValue ClassExtension_setIndexedStaticField(void *ext, int32_t i, RTValue value);
ClojureFunction *ClassExtension_resolveInstanceCall(void *ext, RTValue name, int32_t argCount);
}

#endif
