#include "ThreadsafeCompilerState.h"
#include "../bridge/Exceptions.h"
#include "../tools/EdnParser.h"
#include "../tools/RTValueWrapper.h"

namespace rt {

extern "C" void delete_class_description(void *ptr);

void ThreadsafeCompilerState::storeInternalClasses(RTValue from) {
  // Directly build ClassDescriptions from EDN
  std::vector<ClassDescription> descriptions = buildClasses(from);

  for (auto &desc : descriptions) {
    String *name = String_createDynamicStr(desc.name.c_str());
    String *className = String_createDynamicStr(desc.name.c_str());

    Class *c = Class_create(name, className, 0, NULL);

    // Register by name
    classRegistry.registerObject(desc.name.c_str(), c);

    // Attach the Compiler Metadata
    c->compilerExtension = new ClassDescription(std::move(desc));
    c->compilerExtensionDestructor = delete_class_description;

    // Register by ID if it's a built-in type
    auto *ext = static_cast<ClassDescription *>(c->compilerExtension);
    if (ext->type.isDetermined() && ext->type.determinedType() != classType) {
      Ptr_retain(c);
      classRegistry.registerObject(c, (int32_t)ext->type.determinedType());
    }
  }
}

void ThreadsafeCompilerState::refineClasses() {
  // No longer needed
}

void ThreadsafeCompilerState::storeInternalProtocols(RTValue from) {
  // Protocols can be handled similarly if needed
  release(from);
}

} // namespace rt
