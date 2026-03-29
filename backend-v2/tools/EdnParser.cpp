#include "EdnParser.h"
#include "RTValueWrapper.h"
#include "bridge/Exceptions.h"
#include "runtime/Object.h"
#include "runtime/String.h"
#include <ostream>
#include <unordered_set>

namespace rt {

extern "C" void delete_class_description(void *ptr) {
  delete static_cast<ClassDescription *>(ptr);
}

ClassDescription::~ClassDescription() {
  if (extends)
    Ptr_release(extends);
  for (auto v : staticFieldValues) {
    release(v);
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

TemporaryClassData::TemporaryClassData(RTValue from) { scanMetadata(from); }

void TemporaryClassData::scanMetadata(RTValue from) {
  ConsumedValue root(from);
  if (getType(root.get()) != persistentArrayMapType)
    throwInternalInconsistencyException("Class definitions are not a map");

  PersistentArrayMap *map = (PersistentArrayMap *)RT_unboxPtr(root.get());
  for (uword_t i = 0; i < map->count; i++) {
    RTValue key = map->keys[i];
    RTValue value = map->values[i];

    if (getType(key) != symbolType)
      throwInternalInconsistencyException(
          "Class definition key is not a symbol.");
    if (getType(value) != persistentArrayMapType)
      throwInternalInconsistencyException(
          "Class definition value is not a map.");

    // PersistentArrayMap_assoc consumes its arguments.
    // Protect borrowed value and key for assoc.
    retain(value);
    retain(key);
    RTValue updatedValue = RT_boxPtr(
        PersistentArrayMap_assoc((PersistentArrayMap *)RT_unboxPtr(value),
                                 Keyword_create(String_create("name")), key));

    // updatedValue has refcount 1. wrapped in valWrapper for cleanup.

    PersistentArrayMap *pMap = (PersistentArrayMap *)RT_unboxPtr(updatedValue);

    // PersistentArrayMap_get consumes its arguments.
    // Protect valWrapper for multiple gets.
    Ptr_retain(pMap);
    RTValue classId = PersistentArrayMap_get(
        pMap, Keyword_create(String_create("object-type")));

    if (getType(classId) == nilType) {
      release(classId);
      classId = RT_boxInt32(nextClassId++);
      retain(classId);
      pMap = PersistentArrayMap_assoc(
          pMap, Keyword_create(String_create("object-type")), classId);
      updatedValue = RT_boxPtr(pMap);
    }
    ConsumedValue valWrapper(updatedValue);
    ConsumedValue classIdWrapper(classId);

    if (getType(classIdWrapper.get()) != integerType) {
      throwInternalInconsistencyException(":object-type must be an integer.");
    }

    Ptr_retain(pMap); // for storage
    classesById[RT_unboxInt32(classIdWrapper.get())] = pMap;

    Ptr_retain(pMap); // for get
    RTValue alias =
        PersistentArrayMap_get(pMap, Keyword_create(String_create("alias")));
    ConsumedValue aliasWrapper(alias);

    if (getType(aliasWrapper.get()) == keywordType) {
      // toString consumes.
      String *ss = String_compactify(::toString(aliasWrapper.take()));
      Ptr_retain(pMap); // for storage
      classesByName[string(String_c_str(ss))] = pMap;
      Ptr_release(ss);
    } else if (getType(aliasWrapper.get()) != nilType) {
      throwInternalInconsistencyException(":alias must be a keyword.");
    }

    // toString consumes. Protect borrowed key.
    retain(key);
    String *s = String_compactify(::toString(key));
    Ptr_retain(pMap); // for storage
    classesByName[string(String_c_str(s))] = pMap;
    Ptr_release(s);
  }
}

ClassDescription::ClassDescription(RTValue from,
                                   TemporaryClassData &classData) {
  ConsumedValue root(from);
  this->extends = nullptr;

  if (getType(root.get()) != persistentArrayMapType) {
    throwInternalInconsistencyException(
        "ClassDescription: requires a map as an argument");
  }
  PersistentArrayMap *description =
      (PersistentArrayMap *)RT_unboxPtr(root.get());

  retain(root.get());
  ConsumedValue aliasWrapper(PersistentArrayMap_get(
      description, Keyword_create(String_create("alias"))));

  if (getType(aliasWrapper.get()) == keywordType) {
    String *s = String_compactify(::toString(aliasWrapper.take()));
    string sAlias = String_c_str(s);
    Ptr_release(s);

    if (sAlias == ":nil") {
      this->type = nilType;
    } else {
      // Fallback to integerType if it has one, or classType
      retain(root.get());
      ConsumedValue typeWrapper(PersistentArrayMap_get(
          description, Keyword_create(String_create("object-type"))));
      if (getType(typeWrapper.get()) == integerType) {
        this->type = (objectType)RT_unboxInt32(typeWrapper.get());
      } else {
        throwInternalInconsistencyException(
            "Each object hast to have an integer :object-type");
      }
    }
  } else {
    retain(root.get());
    ConsumedValue typeWrapper(PersistentArrayMap_get(
        description, Keyword_create(String_create("object-type"))));

    if (getType(typeWrapper.get()) == integerType) {
      this->type = (objectType)RT_unboxInt32(typeWrapper.get());
    } else if (getType(typeWrapper.get()) != nilType) {
      throwInternalInconsistencyException("Object-type is not an integer");
    }
  }

  retain(root.get());
  ConsumedValue nameWrapper(PersistentArrayMap_get(
      description, Keyword_create(String_create("name"))));

  if (getType(nameWrapper.get()) != stringType &&
      getType(nameWrapper.get()) != symbolType) {
    throwInternalInconsistencyException("Name is not a string or symbol");
  }

  // toString consumes.
  String *compactifiedName = String_compactify(::toString(nameWrapper.take()));
  this->name = String_c_str(compactifiedName);
  Ptr_release(compactifiedName);

  retain(root.get());
  ConsumedValue extendsWrapper(PersistentArrayMap_get(
      description, Keyword_create(String_create("extends"))));
  if (getType(extendsWrapper.get()) == symbolType ||
      getType(extendsWrapper.get()) == stringType) {
    String *sParent = String_compactify(::toString(extendsWrapper.take()));
    this->parentName = String_c_str(sParent);
    Ptr_release(sParent);
  }

  retain(root.get());
  ConsumedValue sfWrapper(PersistentArrayMap_get(
      description, Keyword_create(String_create("static-fields"))));

  if (getType(sfWrapper.get()) == persistentArrayMapType) {
    this->staticFields = parseStaticFields(sfWrapper.take());
    // Create stable indexing
    for (auto const &[key, val] : this->staticFields) {
      staticFieldIndices[key] = (int32_t)staticFieldValues.size();
      staticFieldValues.push_back(val);
    }
  } else if (getType(sfWrapper.get()) != nilType) {
    throwInternalInconsistencyException(":static-fields must be a map.");
  }

  retain(root.get());
  ConsumedValue ifieldsWrapper(PersistentArrayMap_get(
      description, Keyword_create(String_create("instance-fields"))));

  if (getType(ifieldsWrapper.get()) == persistentVectorType) {
    PersistentVector *vec =
        (PersistentVector *)RT_unboxPtr(ifieldsWrapper.get());
    for (uword_t i = 0; i < vec->count; i++) {
      retain(RT_boxPtr(vec)); // PersistentVector_nth consumes self
      ConsumedValue fieldName(PersistentVector_nth(vec, i));
      // toString consumes.
      String *ss = String_compactify(::toString(fieldName.take()));
      this->instanceFields[String_c_str(ss)] = (int32_t)i;
      Ptr_release(ss);
    }
  }

  retain(root.get());
  ConsumedValue interfacesWrapper(PersistentArrayMap_get(
      description, Keyword_create(String_create("interfaces"))));

  if (getType(interfacesWrapper.get()) == persistentVectorType) {
    // These will be resolved later or stored as names.
    // For now, let's assume classData can help or we store them as symbols.
    // Actually, the previous implementation had ImplementedInterface**
    // implementedInterfaces; which were resolved Classes.
  }

  retain(root.get());
  ConsumedValue sfnsWrapper(PersistentArrayMap_get(
      description, Keyword_create(String_create("static-fns"))));

  // sfnsWrapper is consumed by parseIntrinsics.
  if (getType(sfnsWrapper.get()) == persistentArrayMapType) {
    this->staticFns =
        parseIntrinsics(sfnsWrapper.take(), classData, this->type);
  } else if (getType(sfnsWrapper.get()) != nilType) {
    throwInternalInconsistencyException(":static-fns must be a map.");
  }

  retain(root.get());
  ConsumedValue ifnsWrapper(PersistentArrayMap_get(
      description, Keyword_create(String_create("instance-fns"))));

  if (getType(ifnsWrapper.get()) == persistentArrayMapType) {
    this->instanceFns =
        parseIntrinsics(ifnsWrapper.take(), classData, this->type, true);
  } else if (getType(ifnsWrapper.get()) != nilType) {
    throwInternalInconsistencyException(":instance-fns must be a map.");
  }

  retain(root.get());
  ConsumedValue ctorWrapper(PersistentArrayMap_get(
      description, Keyword_create(String_create("constructor"))));

  if (getType(ctorWrapper.get()) == persistentVectorType) {
    this->constructors =
        parseConstructorDescriptions(ctorWrapper.take(), classData, this->type);
  } else if (getType(ctorWrapper.get()) != nilType) {
    throwInternalInconsistencyException(":constructor must be a vector.");
  }
}

extern "C" {
int32_t ClassExtension_fieldIndex(void *ext, RTValue field) {
  ClassDescription *description = (ClassDescription *)ext;
  String *s = String_compactify(::toString(field));
  string key = String_c_str(s);
  Ptr_release(s);

  auto it = description->instanceFields.find(key);
  if (it != description->instanceFields.end()) {
    return it->second;
  }
  return -1;
}

int32_t ClassExtension_staticFieldIndex(void *ext, RTValue staticField) {
  ClassDescription *description = (ClassDescription *)ext;
  String *s = String_compactify(::toString(staticField));
  string key = String_c_str(s);
  Ptr_release(s);

  auto it = description->staticFieldIndices.find(key);
  if (it != description->staticFieldIndices.end()) {
    return it->second;
  }
  return -1;
}

RTValue ClassExtension_getIndexedStaticField(void *ext, int32_t i) {
  ClassDescription *description = (ClassDescription *)ext;
  if (i >= 0 && i < (int32_t)description->staticFieldValues.size()) {
    RTValue val = description->staticFieldValues[i];
    retain(val);
    return val;
  }
  return RT_boxNil();
}

RTValue ClassExtension_setIndexedStaticField(void *ext, int32_t i,
                                             RTValue value) {
  ClassDescription *description = (ClassDescription *)ext;
  if (i >= 0 && i < (int32_t)description->staticFieldValues.size()) {
    RTValue old = description->staticFieldValues[i];
    description->staticFieldValues[i] = value;
    // Value is retained by the caller or we should retain it here?
    // Class_setIndexedStaticField in C code did NOT retain 'value' but released
    // 'oldValue'. RTValue oldValue = self->staticFields[i];
    // self->staticFields[i] = value;
    // release(oldValue);
    // return value;
    release(old);
    return value;
  }
  return value;
}

ClojureFunction *ClassExtension_resolveInstanceCall(void *ext, RTValue name,
                                                    int32_t argCount) {
  ClassDescription *description = (ClassDescription *)ext;
  String *s = String_compactify(::toString(name));
  string key = String_c_str(s);
  Ptr_release(s);

  auto it = description->instanceFns.find(key);
  if (it != description->instanceFns.end()) {
    // FIXME: We need to resolve these to ClojureFunction* objects.
    /*
    for (auto const& intrinsic : it->second) {
      // We need an actual ClojureFunction object here.
      // This refactoring assumes that compile-time metadata can resolve to
      // runtime functions. For now, if it's a :call type, we return the
      // function. Wait, IntrinsicDescription only has the symbol name. The
      // actual ClojureFunction should be built or cached.
    }
    */
  }
  return nullptr;
}
}

unordered_map<string, RTValue>
ClassDescription::parseStaticFields(RTValue from) {
  ConsumedValue root(from);
  unordered_map<string, RTValue> retVal;
  if (getType(root.get()) != persistentArrayMapType)
    throwInternalInconsistencyException("Static fields are not a map");

  PersistentArrayMap *map = (PersistentArrayMap *)RT_unboxPtr(root.get());
  for (uword_t i = 0; i < map->count; i++) {
    RTValue key = map->keys[i];
    RTValue value = map->values[i];
    if (getType(key) != symbolType)
      throwInternalInconsistencyException("Static fields key is not a symbol.");

    // toString consumes. Protect borrowed key.
    retain(key);
    String *ss = String_compactify(::toString(key));
    string sKey = String_c_str(ss);
    Ptr_release(ss);

    // Value will be owned by staticFields (ClassDescription destructor releases
    // it).
    retain(value);
    retVal[sKey] = value;
  }

  return retVal;
}

unordered_map<string, vector<IntrinsicDescription>>
ClassDescription::parseIntrinsics(RTValue from, TemporaryClassData &classData,
                                  const ObjectTypeSet &thisType,
                                  bool isInstance) {
  ConsumedValue root(from);
  unordered_map<string, vector<IntrinsicDescription>> retVal;
  if (getType(root.get()) != persistentArrayMapType) {
    throwInternalInconsistencyException("Intrinsic collection is not a map");
  }

  PersistentArrayMap *map = (PersistentArrayMap *)RT_unboxPtr(root.get());
  for (uword_t i = 0; i < map->count; i++) {
    RTValue key = map->keys[i];
    RTValue value = map->values[i];
    if (getType(key) != symbolType) {
      throwInternalInconsistencyException(
          "Intrinsic collection key is not a symbol.");
    }

    if (getType(value) == nilType)
      continue;

    if (getType(value) != persistentVectorType) {
      throwInternalInconsistencyException(
          "Intrinsic collection value is not a vector");
    }

    vector<IntrinsicDescription> descriptions;
    PersistentVector *vec = (PersistentVector *)RT_unboxPtr(value);

    uword_t count = vec->count; // Safe borrowed access.

    // PersistentVector_iterator does not consume vec.
    PersistentVectorIterator it = PersistentVector_iterator(vec);

    for (uword_t j = 0; j < count; j++) {
      RTValue intrinsicRaw = PersistentVector_iteratorGet(&it);
      // IntrinsicDescription constructor consumes. Protect borrowed entry.
      retain(intrinsicRaw);
      descriptions.push_back(
          IntrinsicDescription(intrinsicRaw, classData, thisType, isInstance));
      PersistentVector_iteratorNext(&it);
    }

    // toString consumes. Protect borrowed key for the map.
    retain(key);
    String *ss = String_compactify(::toString(key));
    string sKey = String_c_str(ss);
    Ptr_release(ss);

    retVal[sKey] = descriptions;
  }

  return retVal;
}

vector<IntrinsicDescription>
ClassDescription::parseConstructorDescriptions(RTValue from,
                                               TemporaryClassData &classData,
                                               const ObjectTypeSet &thisType) {
  ConsumedValue root(from);
  vector<IntrinsicDescription> retVal;
  if (getType(root.get()) != persistentVectorType) {
    throwInternalInconsistencyException(
        "Constructor collection is not a vector");
  }

  PersistentVector *vec = (PersistentVector *)RT_unboxPtr(root.get());
  uword_t count = vec->count;
  PersistentVectorIterator it = PersistentVector_iterator(vec);

  for (uword_t j = 0; j < count; j++) {
    RTValue intrinsicRaw = PersistentVector_iteratorGet(&it);
    retain(intrinsicRaw);
    IntrinsicDescription desc(intrinsicRaw, classData, thisType, false,
                              thisType);

    // Validation: If returns was provided explicitly, it MUST be exactly
    // thisType.
    if (desc.returnsProvided && desc.returnType != thisType) {
      throwInternalInconsistencyException(
          "Constructor must return exactly the class type it belongs to. "
          "Expected: " +
          thisType.toString() + " Got: " + desc.returnType.toString());
    }

    retVal.push_back(desc);
    PersistentVector_iteratorNext(&it);
  }

  return retVal;
}

IntrinsicDescription::IntrinsicDescription(
    RTValue from, TemporaryClassData &classData, const ObjectTypeSet &thisType,
    bool isInstance, const ObjectTypeSet &defaultReturnType) {
  this->thisType = thisType;
  this->isInstance = isInstance;
  this->returnsProvided = false;
  ConsumedValue root(from);
  if (getType(root.get()) != persistentArrayMapType) {
    throwInternalInconsistencyException("Intrinsic description is not a map");
  }

  PersistentArrayMap *map = (PersistentArrayMap *)RT_unboxPtr(root.get());

  retain(root.get());
  RTValue typeRaw =
      PersistentArrayMap_get(map, Keyword_create(String_create("type")));
  ConsumedValue typeWrapper(typeRaw);

  if (getType(typeWrapper.get()) != keywordType) {
    throwInternalInconsistencyException("Intrinsic :type is not a keyword.");
  }

  // toString consumes.
  String *typeStr = String_compactify(::toString(typeWrapper.take()));
  string sType = String_c_str(typeStr);
  Ptr_release(typeStr);

  if (sType == ":call") {
    this->type = CallType::Call;
  } else if (sType == ":intrinsic") {
    this->type = CallType::Intrinsic;
  } else {
    throwInternalInconsistencyException(
        "Intrinsic :type must be :call or :intrinsic");
  }

  retain(root.get());
  RTValue symbolRaw =
      PersistentArrayMap_get(map, Keyword_create(String_create("symbol")));

  ConsumedValue symbolWrapper(symbolRaw);

  if (getType(symbolWrapper.get()) != stringType) {
    throwInternalInconsistencyException("Intrinsic :symbol is not a string.");
  }

  // symbolWrapper is consumed by String_compactify.
  String *symbolStr =
      String_compactify((String *)RT_unboxPtr(symbolWrapper.take()));
  this->symbol = String_c_str(symbolStr);
  Ptr_release(symbolStr);

  retain(root.get());
  RTValue argsRaw =
      PersistentArrayMap_get(map, Keyword_create(String_create("args")));
  ConsumedValue argsWrapper(argsRaw);

  if (getType(argsWrapper.get()) != persistentVectorType &&
      getType(argsWrapper.get()) != nilType) {
    throwInternalInconsistencyException("Intrinsic :args is not a vector.");
  }

  if (getType(argsWrapper.get()) == persistentVectorType) {
    PersistentVector *argsVector =
        (PersistentVector *)RT_unboxPtr(argsWrapper.get());

    uword_t argCnt = argsVector->count; // Safe borrowed access.

    // PersistentVector_iterator does not consume argsVector.
    bool firstArgIsThis = false;
    PersistentVectorIterator it = PersistentVector_iterator(argsVector);
    for (uword_t j = 0; j < argCnt; j++) {
      RTValue argRaw = PersistentVector_iteratorGet(&it);
      retain(argRaw);
      String *argStr = String_compactify(::toString(argRaw));
      string sArgKey = String_c_str(argStr);
      Ptr_release(argStr);

      if (sArgKey == ":this") {
        if (!isInstance) {
          throwInternalInconsistencyException(
              ":this used in a static function/intrinsic.");
        }
        if (j != 0) {
          throwInternalInconsistencyException(
              ":this must be the first argument.");
        }
        this->argTypes.push_back(thisType);
        firstArgIsThis = true;
      } else if (sArgKey == ":any") {
        this->argTypes.push_back(ObjectTypeSet::all());
      } else if (sArgKey == ":nil") {
        this->argTypes.push_back(ObjectTypeSet(nilType));
      } else {
        if (classData.classesByName.find(sArgKey) ==
            classData.classesByName.end()) {
          throwInternalInconsistencyException("Unknown arg type: " + sArgKey);
        }
        PersistentArrayMap *argClassMap = classData.classesByName[sArgKey];

        Ptr_retain(argClassMap);
        RTValue argTypeRaw = PersistentArrayMap_get(
            argClassMap, Keyword_create(String_create("object-type")));
        ConsumedValue atWrapper(argTypeRaw);

        if (getType(atWrapper.get()) == integerType) {
          objectType t = (objectType)RT_unboxInt32(atWrapper.get());
          this->argTypes.push_back(ObjectTypeSet(t));
        } else {
          this->argTypes.push_back(ObjectTypeSet(classType));
        }
      }
      PersistentVector_iteratorNext(&it);
    }

    if (isInstance && !firstArgIsThis) {
      RTValue v = root.get();
      retain(v);
      String *s = String_compactify(::toString(v));
      string sVal = String_c_str(s);
      Ptr_release(s);
      throwInternalInconsistencyException(
          "Instance method missing :this argument: " + sVal);
    }
  } else if (isInstance) {
    RTValue v = root.get();
    retain(v);
    String *s = String_compactify(::toString(v));
    string sVal = String_c_str(s);
    Ptr_release(s);
    throwInternalInconsistencyException(
        "Instance method missing :this argument (no args vector): " + sVal);
  }

  retain(root.get());
  RTValue returnsRaw =
      PersistentArrayMap_get(map, Keyword_create(String_create("returns")));
  ConsumedValue returnsWrapper(returnsRaw);

  if (getType(returnsWrapper.get()) != nilType) {
    this->returnsProvided = true;
    // toString consumes.
    String *retStr = String_compactify(::toString(returnsWrapper.take()));
    string sRetKey = String_c_str(retStr);
    Ptr_release(retStr);

    if (sRetKey == ":any") {
      this->returnType = ObjectTypeSet::all();
    } else if (sRetKey == ":nil") {
      this->returnType = ObjectTypeSet(nilType);
    } else if (classData.classesByName.find(sRetKey) !=
               classData.classesByName.end()) {
      PersistentArrayMap *retClassMap = classData.classesByName[sRetKey];

      Ptr_retain(retClassMap);
      RTValue retTypeRaw = PersistentArrayMap_get(
          retClassMap, Keyword_create(String_create("object-type")));
      ConsumedValue rtWrapper(retTypeRaw);

      if (getType(rtWrapper.get()) == integerType) {
        objectType t = (objectType)RT_unboxInt32(rtWrapper.get());
        this->returnType = ObjectTypeSet(t);
      } else {
        this->returnType = ObjectTypeSet(classType);
      }
    } else {
      throwInternalInconsistencyException("Unknown return type: " + sRetKey);
    }
  } else {
    this->returnType = defaultReturnType;
  }
}

vector<unique_ptr<ClassDescription>> buildClasses(RTValue from) {
  // from is consumed by TemporaryClassData
  TemporaryClassData classData(from);
  vector<unique_ptr<ClassDescription>> retVal;

  unordered_set<PersistentArrayMap *> processed;

  for (auto const &pair : classData.classesByName) {
    PersistentArrayMap *map = pair.second;
    if (processed.find(map) == processed.end()) {
      processed.insert(map);
      Ptr_retain(map);
      RTValue mapVal = RT_boxPtr(map);
      auto desc = make_unique<ClassDescription>(mapVal, classData);
      retVal.push_back(std::move(desc));
    }
  }
  return retVal;
}

string IntrinsicDescription::toString() const {
  stringstream ss;
  ss << "{:symbol \"" << symbol << "\", :type ";
  switch (type) {
  case CallType::Call:
    ss << ":call";
    break;
  case CallType::Intrinsic:
    ss << ":intrinsic";
    break;
  case CallType::ClojureFn:
    ss << ":clojure-fn";
    break;
  }
  ss << ", :args [";
  for (size_t i = 0; i < argTypes.size(); ++i) {
    if (isInstance && i == 0 && argTypes[i] == thisType) {
      ss << ":this";
    } else {
      ss << argTypes[i].toString();
    }
    ss << (i == argTypes.size() - 1 ? "" : " ");
  }
  ss << "], :returns " << returnType.toString() << "}";
  return ss.str();
}

string ClassDescription::toString() const {
  stringstream ss;
  ss << "{" << name << "\n";
  ss << "  {:object-type " << type.toString();
  if (!parentName.empty()) {
    ss << ",\n   :extends " << parentName;
  }

  if (!staticFields.empty()) {
    ss << ",\n   :static-fields {";
    for (auto it = staticFields.begin(); it != staticFields.end(); ++it) {
      RTValue v = it->second;
      retain(v);
      String *s = String_compactify(::toString(v));
      ss << ":" << it->first << " " << String_c_str(s)
         << (std::next(it) == staticFields.end() ? "" : ", ");
      Ptr_release(s);
    }
    ss << "}";
  }

  if (!instanceFields.empty()) {
    ss << ",\n   :instance-fields [";
    vector<string> fields(instanceFields.size());
    for (auto const &[name, index] : instanceFields) {
      if (index >= 0 && index < (int32_t)fields.size())
        fields[index] = name;
    }
    for (size_t i = 0; i < fields.size(); ++i) {
      ss << "\"" << fields[i] << "\"" << (i == fields.size() - 1 ? "" : " ");
    }
    ss << "]";
  }

  if (!staticFns.empty()) {
    ss << ",\n   :static-fns {";
    for (auto it = staticFns.begin(); it != staticFns.end(); ++it) {
      ss << it->first << " [";
      for (size_t i = 0; i < it->second.size(); ++i) {
        ss << it->second[i].toString()
           << (i == it->second.size() - 1 ? "" : " ");
      }
      ss << "]" << (std::next(it) == staticFns.end() ? "" : ", ");
    }
    ss << "}";
  }

  if (!instanceFns.empty()) {
    ss << ",\n   :instance-fns {";
    for (auto it = instanceFns.begin(); it != instanceFns.end(); ++it) {
      ss << it->first << " [";
      for (size_t i = 0; i < it->second.size(); ++i) {
        ss << it->second[i].toString()
           << (i == it->second.size() - 1 ? "" : " ");
      }
      ss << "]" << (std::next(it) == instanceFns.end() ? "" : ", ");
    }
    ss << "}";
  }

  if (!constructors.empty()) {
    ss << ",\n   :constructor [";
    for (size_t i = 0; i < constructors.size(); ++i) {
      ss << constructors[i].toString()
         << (i == constructors.size() - 1 ? "" : " ");
    }
    ss << "]";
  }

  ss << "}}";
  return ss.str();
}

} // namespace rt
