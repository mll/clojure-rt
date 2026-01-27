#include "ThreadsafeCompilerState.h"
#include "../bridge/Exceptions.h"
#include "../RuntimeHeaders.h"

namespace rt {

void ThreadsafeCompilerState::storeInternalClasses(RTValue from) {
  if (getType(from) != persistentArrayMapType)
    throwInternalInconsistencyException("Class definitions are not a map");
  PersistentArrayMap *map = (PersistentArrayMap *)RT_unboxPtr(from);
  for (uword_t i = 0; i < map->count; i++) {
    RTValue key = map->keys[i];
    RTValue value = map->values[i];
    if (getType(key) != symbolType)
      throwInternalInconsistencyException(
          "Class definition key is not a symbol.");
    if (getType(value) != persistentArrayMapType)
      throwInternalInconsistencyException(
          "Class definition value is not a map.");

    retain(value);
    RTValue classId =
        PersistentArrayMap_get((PersistentArrayMap *)RT_unboxPtr(value),
                               Keyword_create(String_create("object-type")));

    
    
    if (getType(classId) == integerType) {
      retain(value);
      internalClassRegistry.registerObject((PersistentArrayMap *)RT_unboxPtr(value), RT_unboxInt32(classId));
    } else {
      if(getType(classId) != nilType)
        throwInternalInconsistencyException(":object-type must be an integer.");
    }
    
    release(classId);
    retain(key);
    String *s = String_compactify(toString(key));

    retain(value);
    internalClassRegistry.registerObject(String_c_str(s), (PersistentArrayMap *)RT_unboxPtr(value));
    Ptr_release(s);
  }    
  release(from);
}

void ThreadsafeCompilerState::storeInternalProtocols(RTValue from) {
  if (getType(from) != persistentArrayMapType)
    throwInternalInconsistencyException("Protocol definitions are not a map");

  PersistentArrayMap *map = (PersistentArrayMap *)RT_unboxPtr(from);
  for (uword_t i = 0; i < map->count; i++) {
    RTValue key = map->keys[i];
    RTValue value = map->values[i];
    if (getType(key) != symbolType)
      throwInternalInconsistencyException(
          "Protocol definition key is not a symbol.");
    if (getType(value) != persistentArrayMapType)
      throwInternalInconsistencyException(
          "Protocol definition value is not a map.");
   
    retain(key);
    String *s = String_compactify(toString(key));

    retain(value);
    internalProtocolRegistry.registerObject(String_c_str(s), (PersistentArrayMap *)RT_unboxPtr(value));
    Ptr_release(s);
  }    
  release(from);
}



} // namespace rt
    
    
