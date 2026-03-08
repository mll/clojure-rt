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
  retain(from);
  String *s = String_compactify(toString(from));
  printf("!!! from: '''%s'''\n", String_c_str(s));
  Ptr_release(s);

  if (getType(from) != persistentArrayMapType) {
    release(from);
    throwInternalInconsistencyException(
        "ClassDescription: requires a map as an argument");
  }
  PersistentArrayMap *description = (PersistentArrayMap *)RT_unboxPtr(from);
  Ptr_retain(description);

  RTValue typeRaw = PersistentArrayMap_get(
      description, Keyword_create(String_create("object-type")));

  if (getType(typeRaw) == integerType) {
    this->type = (objectType)RT_unboxInt32(typeRaw);
  } else if (getType(typeRaw) == nilType) {
    this->type = classType;
  } else {
    release(typeRaw);
    Ptr_release(description);
    throwInternalInconsistencyException("Object-type is not an integer");
  }
  release(typeRaw);

  Ptr_retain(description);

  RTValue nameRaw = PersistentArrayMap_get(
      description, Keyword_create(String_create("name")));

  if (getType(nameRaw) != stringType && getType(nameRaw) != symbolType) {
    release(nameRaw);
    Ptr_release(description);
    printf("type: %d\n", getType(nameRaw));
    throwInternalInconsistencyException("Name is not a string or symbol");
  }

  String *compactifiedName = String_compactify(toString(nameRaw));
  this->name = String_c_str(compactifiedName);
  Ptr_release(compactifiedName);
  release(nameRaw);

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
  } else {
    release(staticFieldsRaw);
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
  } else {
    release(staticFnsRaw);
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
  } else {
    release(instanceFnsRaw);
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
                                           TemporaryClassData &classData) {
  if (getType(from) != persistentArrayMapType) {
    fprintf(stderr, "Intrinsic collection is not a map\n");
    release(from);
    throwInternalInconsistencyException("Intrinsic collection is not a map");
  }

  PersistentArrayMap *map = (PersistentArrayMap *)RT_unboxPtr(from);

  Ptr_retain(map);
  RTValue typeRaw =
      PersistentArrayMap_get(map, Keyword_create(String_create("type")));
  if (getType(typeRaw) != keywordType) {
    fprintf(stderr, "Intrinsic :type is not a keyword.\n");
    release(typeRaw);
    Ptr_release(map);
    throwInternalInconsistencyException("Intrinsic :type is not a keyword.");
  }
  String *typeStr = String_compactify(toString(typeRaw));
  string sType = String_c_str(typeStr);
  Ptr_release(typeStr);
  release(typeRaw);

  if (sType == ":call") {
    this->type = CallType::Call;
  } else if (sType == ":intrinsic") {
    this->type = CallType::Intrinsic;
  } else {
    fprintf(stderr, "Intrinsic :type must be :call or :intrinsic. Got %s\n",
            sType.c_str());
    Ptr_release(map);
    release(from);
    throwInternalInconsistencyException(
        "Intrinsic :type must be :call or :intrinsic");
  }

  Ptr_retain(map);
  RTValue symbolRaw =
      PersistentArrayMap_get(map, Keyword_create(String_create("symbol")));
  if (getType(symbolRaw) != stringType) {
    fprintf(stderr, "Intrinsic :symbol is not a string.\n");
    release(symbolRaw);
    Ptr_release(map);
    throwInternalInconsistencyException("Intrinsic :symbol is not a string.");
  }
  String *symbolStr = String_compactify((String *)RT_unboxPtr(symbolRaw));
  this->symbol = String_c_str(symbolStr);
  Ptr_release(symbolStr);
  release(symbolRaw);

  Ptr_retain(map);
  RTValue argsRaw =
      PersistentArrayMap_get(map, Keyword_create(String_create("args")));
  if (getType(argsRaw) != persistentVectorType && getType(argsRaw) != nilType) {
    fprintf(stderr, "Intrinsic :args is not a vector.\n");
    release(argsRaw);
    Ptr_release(map);
    throwInternalInconsistencyException("Intrinsic :args is not a vector.");
  }

  if (getType(argsRaw) == persistentVectorType) {
    PersistentVector *argsVector = (PersistentVector *)RT_unboxPtr(argsRaw);
    Ptr_retain(argsVector);
    uword_t argCnt = PersistentVector_count(argsVector);
    PersistentVectorIterator it = PersistentVector_iterator(argsVector);
    for (uword_t j = 0; j < argCnt; j++) {
      RTValue argRaw = PersistentVector_iteratorGet(&it);
      retain(argRaw);
      String *argStr = String_compactify(toString(argRaw));
      string sArgKey = String_c_str(argStr);
      Ptr_release(argStr);
      release(argRaw);

      if (sArgKey == ":this") {
        this->argTypes.push_back(ObjectTypeSet(classType));
      } else if (sArgKey == ":any") {
        this->argTypes.push_back(ObjectTypeSet::dynamicType());
      } else {
        if (classData.classesByName.find(sArgKey) ==
            classData.classesByName.end()) {
          fprintf(stderr, "Unknown arg type: %s\n", sArgKey.c_str());
          Ptr_release(argsVector);
          release(argsRaw);
          Ptr_release(map);
          throwInternalInconsistencyException("Unknown arg type: " + sArgKey);
        }
        PersistentArrayMap *argClassMap = classData.classesByName[sArgKey];
        Ptr_retain(argClassMap);
        RTValue argTypeRaw = PersistentArrayMap_get(
            argClassMap, Keyword_create(String_create("object-type")));
        if (getType(argTypeRaw) == integerType) {
          objectType t = (objectType)RT_unboxInt32(argTypeRaw);
          this->argTypes.push_back(ObjectTypeSet(t));
        } else {
          this->argTypes.push_back(ObjectTypeSet(classType));
        }
        release(argTypeRaw);
      }
      PersistentVector_iteratorNext(&it);
    }
    Ptr_release(argsVector);
  }
  release(argsRaw);

  Ptr_retain(map);
  RTValue returnsRaw =
      PersistentArrayMap_get(map, Keyword_create(String_create("returns")));
  if (getType(returnsRaw) != nilType) {
    retain(returnsRaw);
    String *retStr = String_compactify(toString(returnsRaw));
    string sRetKey = String_c_str(retStr);
    Ptr_release(retStr);

    if (sRetKey == ":any") {
      this->returnType = ObjectTypeSet::dynamicType();
    } else if (classData.classesByName.find(sRetKey) !=
               classData.classesByName.end()) {
      PersistentArrayMap *retClassMap = classData.classesByName[sRetKey];
      Ptr_retain(retClassMap);
      RTValue retTypeRaw = PersistentArrayMap_get(
          retClassMap, Keyword_create(String_create("object-type")));
      if (getType(retTypeRaw) == integerType) {
        objectType t = (objectType)RT_unboxInt32(retTypeRaw);
        this->returnType = ObjectTypeSet(t);
      } else {
        this->returnType = ObjectTypeSet(classType);
      }
      release(retTypeRaw);
    } else {
      fprintf(stderr, "Unknown return type: %s\n", sRetKey.c_str());
      release(returnsRaw);
      Ptr_release(map);
      throwInternalInconsistencyException("Unknown return type: " + sRetKey);
    }
    release(returnsRaw);
  } else {
    this->returnType = ObjectTypeSet::dynamicType();
    release(returnsRaw);
  }

  Ptr_release(map);
}

#include <unordered_set>
vector<ClassDescription> buildClasses(RTValue from) {
  retain(from);
  TemporaryClassData classData(from);
  vector<ClassDescription> retVal;

  unordered_set<PersistentArrayMap *> processed;

  for (auto const &pair : classData.classesByName) {
    PersistentArrayMap *map = pair.second;
    if (processed.find(map) == processed.end()) {
      processed.insert(map);
      Ptr_retain(map);
      RTValue mapVal = RT_boxPtr(map);
      retVal.push_back(ClassDescription(mapVal, classData));
    }
  }

  return retVal;
}

} // namespace rt
