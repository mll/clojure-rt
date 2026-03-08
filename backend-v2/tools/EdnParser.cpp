#include "EdnParser.h"

namespace rt {
ClassDescription::~ClassDescription() {
  for (auto const &s : staticFields) {
    release(s.second);
  }
}

TemporaryClassData::~TemporaryClassData() {
  for (auto const &d : classesByName) {
    Ptr_release(d.second);
  }
  for (auto const &d : classesById) {
    Ptr_release(d.second);
  }
}

TemporaryClassData::TemporaryClassData(RTValue from) {
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
      classesById[RT_unboxInt32(classId)] =
          (PersistentArrayMap *)RT_unboxPtr(value);
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
      classesByName[string(String_c_str(ss))] =
          (PersistentArrayMap *)RT_unboxPtr(value);
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
    classesByName[string(String_c_str(s))] =
        (PersistentArrayMap *)RT_unboxPtr(value);
    Ptr_release(s);
    release(value);
  }
  release(from);
}

ClassDescription::ClassDescription(RTValue from,
                                   TemporaryClassData &classData) {
  if (getType(from) != persistentArrayMapType) {
    release(from);
    throwInternalInconsistencyException(
        "ClassDescription: requires a map as an argument");
  }
  PersistentArrayMap *description = (PersistentArrayMap *)RT_unboxPtr(from);
  Ptr_retain(description);

  RTValue typeRaw = PersistentArrayMap_get(
      description, Keyword_create(String_create("object-type")));

  if (getType(typeRaw) != integerType) {
    release(typeRaw);
    Ptr_release(description);
    throwInternalInconsistencyException("Object-type is not an integer");
  }

  this->type = (objectType)RT_unboxInt32(typeRaw);

  Ptr_retain(description);

  RTValue nameRaw = PersistentArrayMap_get(
      description, Keyword_create(String_create("name")));

  if (getType(nameRaw) != stringType) {
    release(nameRaw);
    Ptr_release(description);
    throwInternalInconsistencyException("Name is not a string");
  }

  String *compactifiedName = String_compactify((String *)RT_unboxPtr(nameRaw));
  this->name = String_c_str(compactifiedName);
  Ptr_release(compactifiedName);

  extends = nullptr;

  Ptr_retain(description);

  RTValue staticFieldsRaw = PersistentArrayMap_get(
      description, Keyword_create(String_create("static-fields")));

  if (getType(staticFieldsRaw) == persistentArrayMapType) {
    try {
      this->staticFields = parseStaticFields(staticFieldsRaw);
    } catch (...) {
      Ptr_release(description);
      throw;
    }
  }

  Ptr_retain(description);

  RTValue staticFnsRaw = PersistentArrayMap_get(
      description, Keyword_create(String_create("static-fns")));

  if (getType(staticFnsRaw) == persistentArrayMapType) {
    try {
      this->staticFns = parseIntrinsics(staticFnsRaw, classData);
    } catch (...) {
      Ptr_release(description);
      throw;
    }
  }

  Ptr_retain(description);

  RTValue instanceFnsRaw = PersistentArrayMap_get(
      description, Keyword_create(String_create("instance-fns")));

  if (getType(instanceFnsRaw) == persistentArrayMapType) {
    try {
      this->instanceFns = parseIntrinsics(instanceFnsRaw, classData);
    } catch (...) {
      Ptr_release(description);
      throw;
    }
  }
  Ptr_release(description);
}

unordered_map<string, RTValue>
ClassDescription::parseStaticFields(RTValue from) {
  unordered_map<string, RTValue> retVal;
  if (getType(from) != persistentArrayMapType)
    throwInternalInconsistencyException("Static fields are not a map");
  PersistentArrayMap *map = (PersistentArrayMap *)RT_unboxPtr(from);
  for (uword_t i = 0; i < map->count; i++) {
    RTValue key = map->keys[i];
    RTValue value = map->values[i];
    if (getType(key) != symbolType)
      throwInternalInconsistencyException("Static fields key is not a symbol.");
    retain(key);
    retain(value);

    String *ss = String_compactify(toString(key));
    string sKey = String_c_str(ss);
    Ptr_release(ss);
    retVal[sKey] = value;
  }

  release(from);
  return retVal;
}

unordered_map<string, vector<IntrinsicDescription>>
ClassDescription::parseIntrinsics(RTValue from, TemporaryClassData &classData) {
  unordered_map<string, vector<IntrinsicDescription>> retVal;
  if (getType(from) != persistentArrayMapType) {
    release(from);
    throwInternalInconsistencyException("Intrinsic collection is not a map");
  }
  PersistentArrayMap *map = (PersistentArrayMap *)RT_unboxPtr(from);
  for (uword_t i = 0; i < map->count; i++) {
    RTValue key = map->keys[i];
    RTValue value = map->values[i];
    if (getType(key) != symbolType) {
      release(from);
      throwInternalInconsistencyException(
          "Intrinsic collection key is not a symbol.");
    }

    RTValue descriptionsRaw = value;
    objectType descriptionsType = getType(descriptionsRaw);

    if (descriptionsType != nilType) {
      if (descriptionsType != persistentVectorType) {
        release(from);
        throwInternalInconsistencyException(
            "Intrinsic collection is not a map");
      }

      vector<IntrinsicDescription> descriptions;

      PersistentVector *descriptionsVector =
          (PersistentVector *)RT_unboxPtr(descriptionsRaw);

      Ptr_retain(descriptionsVector);
      uword_t intrinsicCnt = PersistentVector_count(descriptionsVector);
      PersistentVectorIterator it =
          PersistentVector_iterator(descriptionsVector);
      for (uword_t j = 0; j < intrinsicCnt; j++) {
        RTValue intrinsicRaw = PersistentVector_iteratorGet(&it);
        try {
          descriptions.push_back(IntrinsicDescription(intrinsicRaw, classData));
        } catch (...) {
          release(from);
          throw;
        }

        PersistentVector_iteratorNext(&it);
      }

      retain(key);
      String *ss = String_compactify(toString(key));
      string sKey = String_c_str(ss);
      Ptr_release(ss);

      retVal[sKey] = descriptions;
    }
  }

  release(from);
  return retVal;
}

IntrinsicDescription::IntrinsicDescription(RTValue from,
                                           TemporaryClassData &classData) {}

vector<ClassDescription> buildClasses(RTValue from) {
  return vector<ClassDescription>();
}

} // namespace rt
