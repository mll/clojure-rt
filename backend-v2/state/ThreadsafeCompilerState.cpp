#include "ThreadsafeCompilerState.h"
#include "../bridge/Exceptions.h"
#include "../tools/EdnParser.h"
#include "../tools/RTValueWrapper.h"

namespace rt {

extern "C" void delete_class_description(void *ptr);

void ThreadsafeCompilerState::storeInternalClasses(RTValue from) {
  auto descriptions = buildClasses(from);
  unordered_map<string, ::Class *> localMap;

  // Phase 1: Create all hulls and attach metadata
  for (auto &desc : descriptions) {
    String *nameStr = String_createDynamicStr(desc->name.c_str());
    String *classNameStr = String_createDynamicStr(desc->name.c_str());

    ::Class *c = Class_create(nameStr, classNameStr, 0, NULL);
    Ptr_retain(c); // Local reference for this function's logic
    localMap[desc->name] = c;
    if (desc->type.isDetermined()) {
      c->registerId = desc->type.determinedType();
    }

    c->compilerExtension = desc.release();
    c->compilerExtensionDestructor = delete_class_description;
  }

  // Phase 2: Link inheritance
  for (auto const &pair : localMap) {
    ::Class *c = pair.second;
    ClassDescription *desc =
        static_cast<ClassDescription *>(c->compilerExtension);

    if (!desc->parentName.empty()) {
      ::Class *parentClass = nullptr;
      auto it = localMap.find(desc->parentName);
      if (it != localMap.end()) {
        parentClass = it->second;
        Ptr_retain(parentClass);
      } else {
        // Look in global registry
        parentClass = classRegistry.getCurrent(desc->parentName.c_str());
        // getCurrent already retains for us
      }

      if (parentClass) {
        desc->extends = parentClass;
      }
    }
  }

  // Phase 3: Register and cleanup local references
  for (auto const &pair : localMap) {
    ::Class *c = pair.second;
    ClassDescription *desc =
        static_cast<ClassDescription *>(c->compilerExtension);

    // Give our local reference to the registry (registerObject name-based)
    classRegistry.registerObject(desc->name.c_str(), c);

    if (desc->type.isDetermined()) {
      // If we register by ID too, we need another reference because the ID
      // registry also releases on its own.
      Ptr_retain(c);
      classRegistry.registerObject(c, (int32_t)desc->type.determinedType());
    }

    Ptr_release(c); // Release the local reference
  }
}

void ThreadsafeCompilerState::storeInternalProtocols(RTValue from) {
  // Protocols can be handled similarly if needed
  release(from);
}

} // namespace rt
