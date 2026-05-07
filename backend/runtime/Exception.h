#ifndef RT_EXCEPTION_H
#define RT_EXCEPTION_H

#include "ObjectProto.h"
#include "String.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Exception {
    Object header;
    void *bridgedData;
} Exception;

Exception *Exception_createAssertionErrorWithMessage(String *message);

void Exception_destroy(Exception *self);
uword_t Exception_hash(Exception *self);
bool Exception_equals(Exception *self, Exception *other);
String *Exception_toString(Exception *self);

#ifdef __cplusplus
}
#endif

#endif // RT_EXCEPTION_H
