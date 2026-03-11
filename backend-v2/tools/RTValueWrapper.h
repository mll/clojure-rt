#ifndef RT_VALUE_WRAPPER_H
#define RT_VALUE_WRAPPER_H

#include "../RuntimeHeaders.h"

namespace rt {

/**
 * RAII wrapper for RTValue that ensures it is released (consumed) on
 * destruction. This is useful for maintaining the argument consumption model in
 * C++ code, especially when exceptions are thrown or multiple return paths
 * exist.
 */
class ConsumedValue {
  RTValue value;
  bool consumed;

public:
  explicit ConsumedValue(RTValue v) : value(v), consumed(false) {}

  // Non-copyable
  ConsumedValue(const ConsumedValue &) = delete;
  ConsumedValue &operator=(const ConsumedValue &) = delete;

  // Moveable
  ConsumedValue(ConsumedValue &&other) noexcept
      : value(other.value), consumed(other.consumed) {
    other.consumed = true; // Other no longer owns it
  }

  ~ConsumedValue() {
    if (!consumed) {
      release(value);
    }
  }

  RTValue get() const { return value; }

  /**
   * Takes ownership of the value. The wrapper will no longer release it.
   * Use this when passing the value to another function that will consume it.
   */
  RTValue take() {
    consumed = true;
    return value;
  }

  operator RTValue() const { return value; }
};

/**
 * RAII wrapper for Ptr (Object*) that ensures Ptr_release is called on
 * destruction unless ownership is taken.
 */
template <typename T> class PtrWrapper {
  T *ptr;
  bool consumed;

public:
  explicit PtrWrapper(T *p) : ptr(p), consumed(false) {}

  // Non-copyable
  PtrWrapper(const PtrWrapper &) = delete;
  PtrWrapper &operator=(const PtrWrapper &) = delete;

  // Moveable
  PtrWrapper(PtrWrapper &&other) noexcept
      : ptr(other.ptr), consumed(other.consumed) {
    other.consumed = true;
  }

  ~PtrWrapper() {
    if (!consumed && ptr) {
      Ptr_release(ptr);
    }
  }

  T *get() const { return ptr; }

  T *take() {
    consumed = true;
    return ptr;
  }

  T *operator->() const { return ptr; }
  operator T *() const { return ptr; }
  operator bool() const { return ptr != nullptr; }
};

} // namespace rt

#endif
