#ifndef RT_BRIDGED_OBJECT
#define RT_BRIDGED_OBJECT

#ifdef __cplusplus
extern "C" {
#endif

#include "ObjectProto.h"
#include <stdbool.h>

typedef struct Object Object;
typedef struct String String;

/* Used to allow for C++ objects to be memory managed by the runtime (they might
 * need EBR) */
struct BridgedObject {
  Object super;
  void *contents;
  void *jitEngine;
  void (*destructor)(void *contents, void *jitEngine);
};

typedef struct BridgedObject BridgedObject;

BridgedObject *BridgedObject_create(void *contents,
                                    void (*destructor)(void *, void *),
                                    void *jitEngine);
bool BridgedObject_equals(BridgedObject *self, BridgedObject *other);
uword_t BridgedObject_hash(BridgedObject *self);
String *BridgedObject_toString(BridgedObject *self);
void BridgedObject_destroy(BridgedObject *self);

#ifdef __cplusplus
}
#endif

#endif
