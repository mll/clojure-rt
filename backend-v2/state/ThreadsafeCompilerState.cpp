#include "ThreadsafeCompilerState.h"
#include "../bridge/Exceptions.h"
#include "../tools/EdnParser.h"
#include "../tools/RTValueWrapper.h"

namespace rt {

extern "C" void delete_class_description(void *ptr);
extern "C" _Atomic(uword_t) globalMethodICEpoch;

ThreadsafeCompilerState::~ThreadsafeCompilerState() {}

void ThreadsafeCompilerState::storeInternalClasses(RTValue from) {
  auto descriptions = buildClasses(from);
  unordered_map<string, ::Class *> localMap;

  // Phase 1: Create all hulls and attach metadata
  for (auto &desc : descriptions) {
    String *nameStr = String_createDynamicStr(desc->name.c_str());
    String *classNameStr = String_createDynamicStr(desc->name.c_str());

    ::Class *c = Class_create(nameStr, classNameStr, 0, NULL);
    localMap[desc->name] = c;
    if (desc->type.isDetermined()) {
      c->registerId = desc->type.determinedType();
    }

    c->compilerExtension = desc.release();
    c->compilerExtensionDestructor = delete_class_description;
  }

    try {
      // Phase 2: Link inheritance
      for (auto const &pair : localMap) {
        ::Class *c = pair.second;
        ClassDescription *desc =
            static_cast<ClassDescription *>(c->compilerExtension);

        if (!desc->parentName.empty() || !desc->parentNames.empty()) {
          // Populate runtime Class superclasses
          int32_t count = (int32_t)desc->parentNames.size();
          ClassList *list =
              (ClassList *)allocate(sizeof(ClassList) + sizeof(::Class *) * count);
          list->count = count;
          for (int32_t i = 0; i < count; i++) {
            ::Class *pc = nullptr;
            auto it2 = localMap.find(desc->parentNames[i]);
            if (it2 != localMap.end()) {
              pc = it2->second;
              Ptr_retain(pc);
            } else {
              pc = classRegistry.getCurrent(desc->parentNames[i].c_str());
              if (!pc) {
                pc = protocolRegistry.getCurrent(desc->parentNames[i].c_str());
              }
            }
            if (pc) {
              list->classes[i] = pc;
              if (i == 0) {
                desc->extends = pc;
                Ptr_retain(pc); // Extra retain for desc->extends
              }
            } else {
              throwInternalInconsistencyException(
                  "Parent class/protocol not found: " + desc->parentNames[i]);
            }
          }
          // Atomic store to publish superclasses.
          // Note: The initial empty list from Class_create is leaked here unless we destroy it.
          // But Class_create was called with 0 superclasses, so it allocated an empty ClassList.
          ClassList *oldList = atomic_load_explicit(&c->superclasses, memory_order_relaxed);
          atomic_store_explicit(&c->superclasses, list, memory_order_relaxed);
          if (oldList) deallocate(oldList);
        }
      }

      // Phase 3: Register and cleanup local references
      for (auto const &pair : localMap) {
        ::Class *c = pair.second;
        ClassDescription *desc =
            static_cast<ClassDescription *>(c->compilerExtension);

        Ptr_retain(c); // For name-based registry
        classRegistry.registerObject(desc->name.c_str(), c);

        if (desc->type.isDetermined()) {
          c->registerId = (int32_t)desc->type.determinedType();
        }

        Ptr_retain(c); // For ID-based registry
        if (desc->type.isDetermined()) {
          classRegistry.registerObject(c, c->registerId);
        } else {
          c->registerId = (int32_t)classRegistry.registerObject(c, -1);
        }

        // Release the initial reference from Phase 1
        Ptr_release(c);
      }
    } catch (...) {
      for (auto const &pair : localMap) {
        Ptr_release(pair.second);
      }
      throw;
    }

  // Phase 4: Link implemented protocols
  for (auto const &pair : localMap) {
    ::Class *c = pair.second;
    ClassDescription *desc =
        static_cast<ClassDescription *>(c->compilerExtension);

      if (!desc->implements.empty()) {
        std::vector<::Class *> classImpls;
        for (auto const &[protoName, protoMethods] : desc->implements) {
          ::Class *proto = protocolRegistry.getCurrent(protoName.c_str());
          if (!proto) {
            continue;
          }
          // proto was already retained by getCurrent. Ownership transferred to c->implementedProtocols.
          classImpls.push_back(proto);
        }

        if (!classImpls.empty()) {
          int32_t count = (int32_t)classImpls.size();
          ClassList *list =
              (ClassList *)allocate(sizeof(ClassList) + sizeof(::Class *) * count);
          list->count = count;
          for (int i = 0; i < count; i++) {
            list->classes[i] = classImpls[i];
          }
          atomic_store_explicit(&c->implementedProtocols, list, memory_order_relaxed);
        }
      }
  }

  try {
    std::vector<::Class *> classes;
    for (auto const &pair : localMap) {
      classes.push_back(pair.second);
    }
    validateProtocolImplementations(classes);
  } catch (...) {
    // Note: localMap objects are already registered in classRegistry,
    // so they will be cleaned up by classRegistry's destructor if we
    // let the exception propagate and the caller destroys the state.
    throw;
  }
  atomic_fetch_add_explicit(&globalMethodICEpoch, 1, memory_order_release);
}

void ThreadsafeCompilerState::storeInternalProtocols(RTValue from) {
  auto descriptions = buildClasses(from);
  unordered_map<string, ::Class *> localMap;

  // Phase 1: Create all hulls
  for (auto &desc : descriptions) {
    String *nameStr = String_createDynamicStr(desc->name.c_str());
    String *classNameStr = String_createDynamicStr(desc->name.c_str());
    ::Class *c = Class_createProtocol(nameStr, classNameStr, 0, NULL);
    localMap[desc->name] = c;
    if (desc->type.isDetermined()) {
      c->registerId = desc->type.determinedType();
    }
    c->compilerExtension = desc.release();
    c->compilerExtensionDestructor = delete_class_description;
  }

  try {
    // Phase 2: Link inheritance (protocols can extend other protocols)
    for (auto const &pair : localMap) {
      ::Class *c = pair.second;
      ClassDescription *desc =
          static_cast<ClassDescription *>(c->compilerExtension);

      if (!desc->parentNames.empty()) {
        int32_t count = (int32_t)desc->parentNames.size();
        ClassList *list =
            (ClassList *)allocate(sizeof(ClassList) + sizeof(::Class *) * count);
        list->count = count;
        for (int32_t i = 0; i < count; i++) {
          ::Class *pc = nullptr;
          auto it = localMap.find(desc->parentNames[i]);
          if (it != localMap.end()) {
            pc = it->second;
            Ptr_retain(pc);
          } else {
            pc = protocolRegistry.getCurrent(desc->parentNames[i].c_str());
            if (!pc) {
              pc = classRegistry.getCurrent(desc->parentNames[i].c_str());
            }
          }
          if (pc) {
            list->classes[i] = pc;
            // Class_destroy will release each superclass, so we need a retain
            // here (the one from getCurrent/localMap is consumed here)
            if (i == 0) {
              desc->extends = pc;
              Ptr_retain(pc); // Extra retain for desc->extends which is released in ~ClassDescription
            }
          } else {
            throwInternalInconsistencyException(
                "Extended protocol not found: " + desc->parentNames[i]);
          }
        }
        ClassList *oldList = atomic_load_explicit(&c->superclasses, memory_order_relaxed);
        atomic_store_explicit(&c->superclasses, list, memory_order_relaxed);
        if (oldList) deallocate(oldList);
      }
    }

    // Phase 3: Register
    for (auto const &pair : localMap) {
      ::Class *c = pair.second;
      ClassDescription *desc =
          static_cast<ClassDescription *>(c->compilerExtension);

      Ptr_retain(c); // For name-based registry
      protocolRegistry.registerObject(desc->name.c_str(), c);

      if (desc->type.isDetermined()) {
        c->registerId = (int32_t)desc->type.determinedType();
      }
      Ptr_retain(c); // For ID-based registry
      if (desc->type.isDetermined()) {
        protocolRegistry.registerObject(c, c->registerId);
      } else {
        c->registerId = (int32_t)protocolRegistry.registerObject(c, -1);
      }

      // Release the initial reference from Phase 1
      Ptr_release(c);
    }
  } catch (...) {
    for (auto const &pair : localMap) {
      Ptr_release(pair.second);
    }
    throw;
  }
  atomic_fetch_add_explicit(&globalMethodICEpoch, 1, memory_order_release);
}

void ThreadsafeCompilerState::validateProtocolImplementations(
    const std::vector<::Class *> &classes) {
  // 1. Validate classes
  size_t i = 0;
  try {
    for (; i < classes.size(); i++) {
      ::Class *c = classes[i];
      ClassDescription *desc =
          static_cast<ClassDescription *>(c->compilerExtension);
      if (!desc) {
        continue;
      }

      for (auto const &[protoName, implMethods] : desc->implements) {
        ::Class *proto = protocolRegistry.getCurrent(protoName.c_str());
        if (!proto)
          continue; // Should have been caught earlier

        // Helper to collect all required methods from protocol hierarchy
        unordered_map<string, vector<IntrinsicDescription>> requiredMethods;
        vector<::Class *> queue;
        queue.push_back(proto);
        unordered_set<::Class *> visited;

        try {
          while (!queue.empty()) {
            ::Class *curr = queue.back();
            queue.pop_back();
            if (visited.count(curr)) {
              Ptr_release(curr);
              continue;
            }
            visited.insert(curr);

            ClassDescription *currDesc =
                static_cast<ClassDescription *>(curr->compilerExtension);
            if (currDesc) {
              for (auto const &[name, fns] : currDesc->instanceFns) {
                for (auto const &fn : fns) {
                  requiredMethods[name].push_back(fn);
                }
              }
              // Also add parents of this protocol
              ClassList *supers = atomic_load_explicit(&curr->superclasses, memory_order_acquire);
              if (supers) {
                for (int j = 0; j < supers->count; j++) {
                  ::Class *parent = supers->classes[j];
                  Ptr_retain(parent);
                  queue.push_back(parent);
                }
              }
            }
          }

          // Validate each required method
          for (auto const &[methodName, protoFns] : requiredMethods) {
            auto it = implMethods.find(methodName);
            if (it == implMethods.end()) {
              throwInternalInconsistencyException(
                  "Class " + desc->name +
                  " is missing implementation of method " + methodName +
                  " from protocol " + protoName);
            }

            const auto &implFns = it->second;
            for (auto const &protoFn : protoFns) {
              // Check if there's an implementation for this arity
              bool found = false;
              for (auto const &implFn : implFns) {
                if (implFn.argTypes.size() == protoFn.argTypes.size()) {
                  // Check signature matches
                  for (size_t k = 0; k < protoFn.argTypes.size(); k++) {
                    // The first argument is :this, which will differ between
                    // protocol and implementing class.
                    if (k == 0 && protoFn.isInstance && implFn.isInstance)
                      continue;

                    if (protoFn.argTypes[k] != ObjectTypeSet::dynamicType() &&
                        protoFn.argTypes[k] != implFn.argTypes[k]) {
                      throwInternalInconsistencyException(
                          "Signature mismatch for method " + methodName +
                          " in class " + desc->name + " for protocol " +
                          protoName + ". Arg " + to_string(k) + " expected " +
                          protoFn.argTypes[k].toString() + " got " +
                          implFn.argTypes[k].toString());
                    }
                  }
                  found = true;
                  break;
                }
              }
              if (!found) {
                throwInternalInconsistencyException(
                    "Class " + desc->name + " missing arity " +
                    to_string(protoFn.argTypes.size()) + " for method " +
                    methodName + " of protocol " + protoName);
              }
            }
          }

          // Check for extra methods in :implements
          for (auto const &[methodName, implFns] : implMethods) {
            if (requiredMethods.find(methodName) == requiredMethods.end()) {
              throwInternalInconsistencyException(
                  "Class " + desc->name + " implements extra method " +
                  methodName + " not found in protocol " + protoName +
                  " or its parents");
            }
          }

          // Check for conflicts with regular methods
          for (auto const &[methodName, implFns] : implMethods) {
            auto it = desc->instanceFns.find(methodName);
            if (it != desc->instanceFns.end()) {
              for (auto const &protoFn : implFns) {
                for (auto const &regFn : it->second) {
                  if (regFn.argTypes.size() == protoFn.argTypes.size()) {
                    throwInternalInconsistencyException(
                        "Class " + desc->name + " has regular method " +
                        methodName + " that conflicts with protocol " +
                        protoName + " implementation of the same arity (" +
                        to_string(protoFn.argTypes.size()) + ")");
                  }
                }
              }
            }
          }
        } catch (...) {
          for (auto vproto : visited) {
            Ptr_release(vproto);
          }
          for (auto qproto : queue) {
            Ptr_release(qproto);
          }
          throw;
        }

        // Release all visited protocols (collected in the queue process)
        for (auto vproto : visited) {
          Ptr_release(vproto);
        }
      }
    }
  } catch (...) {
    throw;
  }
}

} // namespace rt
