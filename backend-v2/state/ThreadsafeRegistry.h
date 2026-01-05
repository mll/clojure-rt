#ifndef THREADSAFE_REGISTRY_H
#define THREADSAFE_REGISTRY_H

#include <shared_mutex>
#include <mutex>
#include <unordered_map>
#include <string>
#include <vector>
#include "../RuntimeHeaders.h"
#include "../bridge/Exceptions.h"

namespace rt {
  template <typename T>
  class ThreadsafeRegistry {
  private:
    mutable std::shared_mutex registryMutex;
    std::unordered_map<std::string, const T *> registry;
    std::unordered_map<uword_t, const T *> indexedRegistry;
    uword_t currentIndex = 1;
    bool manageRuntimeMemory;

  public:
    ThreadsafeRegistry(bool _manageRuntimeMemory)
        : manageRuntimeMemory(_manageRuntimeMemory) {}

    uword_t registerObject(const T *newDef) {
      std::unique_lock<std::shared_mutex> lock(registryMutex);
      indexedRegistry[currentIndex] = newDef;
      return ++currentIndex;
    }      
    
    void registerObject(const char *name, const T *newDef) {
      std::unique_lock<std::shared_mutex> lock(registryMutex);
      
      std::string key(name);
      auto it = registry.find(key); 
      
      if (manageRuntimeMemory && it != registry.end()) {
        Ptr_release((void*)it->second); 
      }        
      
      registry[name] = newDef;
    }

    T *getCurrent(const uword_t index) const {
      std::shared_lock<std::shared_mutex> lock(registryMutex);
      auto it = indexedRegistry.find(index);

      if (it != registry.end()) {
        if(manageRuntimeMemory) Ptr_retain((void *)it->second);
        return it->second;
      }

      throwInternalInconsistencyException(
          std::string("There is no definition for ") + std::to_string(index));
      
      return nullptr;
    }      
    
    T *getCurrent(const char *name) const {
      std::shared_lock<std::shared_mutex> lock(registryMutex);
      
      auto it = registry.find(name);

      if (it != registry.end()) {
        if(manageRuntimeMemory) Ptr_retain((void *)it->second);
        return it->second;
      }
      
      throwInternalInconsistencyException(std::string("There is no definition for ") + name);
      
      return nullptr;
    }

    ~ThreadsafeRegistry() {
      if(!manageRuntimeMemory) return;
      std::unique_lock<std::shared_mutex> lock(registryMutex);      
      for (auto const& [name, definition] : registry) {
        if (definition != nullptr) {
          Ptr_release((void*)definition);
        }
      }
    }      
  };
}

#endif
