#ifndef CLEANUP_CHAIN_GUARD_H
#define CLEANUP_CHAIN_GUARD_H

#include <cstddef>
#include "TypedValue.h"
#include "../types/ObjectTypeSet.h"

namespace llvm {
class Value;
}

namespace rt {

class CodeGen;

class CleanupChainGuard {
  CodeGen &cg;
  size_t pushedCount = 0;

public:
  explicit CleanupChainGuard(CodeGen &c);
  ~CleanupChainGuard();

  void push(TypedValue val);
  void popAll();
  void cancel(size_t count) { pushedCount -= count; }

  size_t size() const { return pushedCount; }
  
  // Static helper to check if a type needs protection
  static bool needsProtection(const ObjectTypeSet &type) {
    return !type.isScalar() && !type.isBoxedScalar();
  }

  // Prevent copying
  CleanupChainGuard(const CleanupChainGuard &) = delete;
  CleanupChainGuard &operator=(const CleanupChainGuard &) = delete;
};

} // namespace rt

#endif // CLEANUP_CHAIN_GUARD_H
