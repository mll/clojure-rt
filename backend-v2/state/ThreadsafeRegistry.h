#ifndef THREADSAFE_REGISTRY_H
#define THREADSAFE_REGISTRY_H

#include "../RuntimeHeaders.h"
#include "../bridge/Exceptions.h"
#include "runtime/Object.h"
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace rt {
template <typename T> class ThreadsafeRegistry {
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
    int32_t index = requiredIndex == -1 ? currentIndex++ : requiredIndex;

    auto it = indexedRegistry.find(index);
    if (manageRuntimeMemory && it != indexedRegistry.end()) {
      Ptr_release((void *)it->second);
    }
    indexedRegistry[index] = newDef;

    if (index >= currentIndex) {
      currentIndex = index + 1;
    }
    return index;
  }

  void registerObject(const char *name, T *newDef) {
    std::lock_guard<std::mutex> lock(registryMutex);

    std::string key(name);
    auto it = registry.find(key);

    if (manageRuntimeMemory && it != registry.end()) {
      Ptr_release((void *)it->second);
    }

    registry[name] = newDef;
  }

  template <typename F> T *getOrCreate(const char *name, F &&factory) {
    std::lock_guard<std::mutex> lock(registryMutex);

    std::string key(name);
    auto it = registry.find(key);

    if (it != registry.end()) {
      if (manageRuntimeMemory)
        Ptr_retain((void *)it->second);
      return it->second;
    }

    T *newDef = factory();
    registry[key] = newDef;
    if (manageRuntimeMemory)
      Ptr_retain(newDef);
    return newDef;
  }

  T *getCurrent(const int32_t index) const {
    std::lock_guard<std::mutex> lock(registryMutex);
    auto it = indexedRegistry.find(index);

    if (it != indexedRegistry.end()) {
      if (manageRuntimeMemory)
        Ptr_retain((void *)it->second);
      return it->second;
    }

    return nullptr;
  }

  T *getCurrent(const char *name) const {
    std::lock_guard<std::mutex> lock(registryMutex);

    auto it = registry.find(name);

    if (it != registry.end()) {
      if (manageRuntimeMemory)
        Ptr_retain((void *)it->second);
      return it->second;
    }

    return nullptr;
  }

  ~ThreadsafeRegistry() {
    if (!manageRuntimeMemory)
      return;
    std::lock_guard<std::mutex> lock(registryMutex);
    while (!registry.empty()) {
      auto it = registry.begin();
      T *ptr = it->second;
      registry.erase(it);
      if (ptr != nullptr) {
        Ptr_release((void *)ptr);
      }
    }
    while (!indexedRegistry.empty()) {
      auto it = indexedRegistry.begin();
      T *ptr = it->second;
      indexedRegistry.erase(it);
      if (ptr != nullptr) {
        Ptr_release((void *)ptr);
      }
    }
  }
};
} // namespace rt

#endif
