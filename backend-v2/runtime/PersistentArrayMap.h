#ifndef RT_PERSISTENT_ARRAY_MAP
#define RT_PERSISTENT_ARRAY_MAP

#ifdef __cplusplus
extern "C" {
#endif

#include "RTValue.h"
#include "String.h"
// http://blog.higher-order.net/2009/09/08/understanding-clojures-persistenthashmap-deftwice
// https://groups.google.com/g/clojure/c/o12kFA-j1HA
// https://gist.github.com/mrange/d6e7415113ebfa52ccb660f4ce534dd4
// https://www.infoq.com/articles/in-depth-look-clojure-collections/

typedef struct Object Object;
typedef struct PersistentArrayMap PersistentArrayMap;

struct PersistentArrayMap {
  Object super;
  uword_t count;
  RTValue keys[HASHTABLE_THRESHOLD];
  RTValue values[HASHTABLE_THRESHOLD];
};

PersistentArrayMap *PersistentArrayMap_empty();
void PersistentArrayMap_initialise();
void PersistentArrayMap_cleanup();

bool PersistentArrayMap_equals(PersistentArrayMap *self,
                               PersistentArrayMap *other);
uword_t PersistentArrayMap_hash(PersistentArrayMap *self);
String *PersistentArrayMap_toString(PersistentArrayMap *self);
void PersistentArrayMap_destroy(PersistentArrayMap *self,
                                bool deallocateChildren);
void PersistentArrayMap_promoteToShared(PersistentArrayMap *self,
                                        uword_t current);

PersistentArrayMap *PersistentArrayMap_assoc(PersistentArrayMap *self,
                                             RTValue key, RTValue value);
PersistentArrayMap *PersistentArrayMap_dissoc(PersistentArrayMap *self,
                                              RTValue key);
RTValue PersistentArrayMap_get(PersistentArrayMap *self, RTValue key);
RTValue PersistentArrayMap_dynamic_get(RTValue self, RTValue key);
word_t PersistentArrayMap_indexOf(PersistentArrayMap *self, RTValue key);

PersistentArrayMap *PersistentArrayMap_create();
PersistentArrayMap *PersistentArrayMap_createMany(int32_t pairCount, ...);

#ifdef __cplusplus
}
#endif

#endif
