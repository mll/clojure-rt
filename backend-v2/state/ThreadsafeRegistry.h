#ifndef THREADSAFE_REGISTRY_H
#define THREADSAFE_REGISTRY_H

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
    mutable std::mutex registryMutex;
    std::unordered_map<std::string, T *> registry;
    std::unordered_map<int32_t, T *> indexedRegistry;
    int32_t currentIndex = 10000;
    bool manageRuntimeMemory;

  public:
    ThreadsafeRegistry(bool _manageRuntimeMemory)
        : manageRuntimeMemory(_manageRuntimeMemory) {}

    uword_t registerObject(T *newDef, int32_t requiredIndex = -1) {
      std::lock_guard<std::mutex> lock(registryMutex);
      requiredIndex = requiredIndex == -1 ? currentIndex : requiredIndex;
      auto it = indexedRegistry.find(requiredIndex);
      if (manageRuntimeMemory &&  it != indexedRegistry.end()) {
        Ptr_release((void*)it->second); 
      }                
      indexedRegistry[currentIndex] = newDef;
      return currentIndex++;
    }

    void registerObject(const char *name, T *newDef) {
      std::lock_guard<std::mutex> lock(registryMutex);      
      
      std::string key(name);
      auto it = registry.find(key); 
      
      if (manageRuntimeMemory && it != registry.end()) {
        Ptr_release((void*)it->second); 
      }        
      
      registry[name] = newDef;
    }

    T *getCurrent(const int32_t index) const {
      std::lock_guard<std::mutex> lock(registryMutex);            
      auto it = indexedRegistry.find(index);

      if (it != indexedRegistry.end()) {
        if(manageRuntimeMemory) Ptr_retain((void *)it->second);
        return it->second;
      }
      
      return nullptr;
    }

    T *getCurrent(const char *name) const {
      std::lock_guard<std::mutex> lock(registryMutex);                  
      
      auto it = registry.find(name);

      if (it != registry.end()) {
        if(manageRuntimeMemory) Ptr_retain((void *)it->second);
        return it->second;
      }
      
      return nullptr;
    }

    ~ThreadsafeRegistry() {
      if (!manageRuntimeMemory)
        return;
      std::lock_guard<std::mutex> lock(registryMutex);                  
      for (auto const& [name, definition] : registry) {
        if (definition != nullptr) {
          Ptr_release((void*)definition);
        }
      }

      for (auto const& [index, definition] : indexedRegistry) {
        if (definition != nullptr) {
          Ptr_release((void*)definition);
        }
      }      
    }      
  };
}

#endif
