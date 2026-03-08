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

    retain(key);
    retain(value);

    value = RT_boxPtr(
        PersistentArrayMap_assoc((PersistentArrayMap *)RT_unboxPtr(value),
                                 Keyword_create(String_create("name")), key));        
                                     
    retain(value);
    RTValue classId =
        PersistentArrayMap_get((PersistentArrayMap *)RT_unboxPtr(value),
                               Keyword_create(String_create("object-type")));
    
    if (getType(classId) == integerType) {
      retain(value);
      internalClassRegistry.registerObject((PersistentArrayMap *)RT_unboxPtr(value), RT_unboxInt32(classId));
    } else {
      if (getType(classId) != nilType) {
        release(classId);
        release(key);
        release(value);
        throwInternalInconsistencyException(":object-type must be an integer.");
      }
    }

    release(classId);

    retain(value);
    RTValue alias =
        PersistentArrayMap_get((PersistentArrayMap *)RT_unboxPtr(value),
                               Keyword_create(String_create("alias")));

    /* Aliases are stored in the same way as classes themselves - we abuse
       alias string representation that starts with a colon */
    
    if (getType(alias) == keywordType) {
     String *ss = String_compactify(toString(alias));
     retain(value);
     internalClassRegistry.registerObject(String_c_str(ss), (PersistentArrayMap *)RT_unboxPtr(value));
     Ptr_release(ss);
    } else {
      if (getType(alias) != nilType) {
        release(alias);
        release(key);
        release(value);
        throwInternalInconsistencyException(":alias must be a keyword.");
      }
    }
    
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
    
    
