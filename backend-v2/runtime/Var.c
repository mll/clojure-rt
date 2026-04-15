#include "Var.h"
#include "Ebr.h"
#include "Object.h"
#include "RTValue.h"
#include "String.h"
#include "word.h"
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#if defined(__x86_64__) || defined(__i386__)
/* Works for both Intel Macs and Linux on x86 */
#include <immintrin.h>
#define CPU_PAUSE() _mm_pause()
#elif defined(__aarch64__) || defined(__arm__)
/* Works for Apple Silicon (M1/M2/M3) and ARM Linux */
#if defined(__GNUC__) || defined(__clang__)
#define CPU_PAUSE() __asm__ __volatile__("yield")
#else
/* Fallback for other compilers on ARM */
#define CPU_PAUSE() __builtin_arm_yield()
#endif
#else
/* No-op for unknown architectures */
#define CPU_PAUSE()                                                            \
  do {                                                                         \
  } while (0)
#endif

Var *Var_create(RTValue keyword) {
  Var *self = (Var *)allocate(sizeof(Var));
  atomic_store_explicit(&(self->root), RT_boxNull(), memory_order_relaxed);
  self->dynamic = false;
  self->keyword = keyword;
  atomic_store_explicit(&self->rev, 0, memory_order_relaxed);
  Object_create((Object *)self, varType);
  // Var is always shared
  atomic_store_explicit(&(((Object *)self)->atomicRefCount),
                        COUNT_INC | SHARED_BIT, memory_order_release);
  return self;
};

bool Var_equals(Var *self, Var *other) {
  return false; // pointer equality in Object_equals
};

uword_t Var_hash(Var *self) { return hash(self->keyword); };

String *Var_toString(Var *self) {
  String *retVal = String_create("#");
  retVal = String_concat(retVal, String_replace(toString(self->keyword),
                                                String_create(":"),
                                                String_create("'")));
  Ptr_release(self);
  return retVal;
};

void Var_destroy(Var *self) {
  RTValue oldRoot = atomic_load_explicit(&self->root, memory_order_relaxed);
  if (oldRoot != RT_boxNull()) {
    autorelease(oldRoot);
  }
  release(self->keyword);
};

Var *Var_setDynamic(Var *self, bool dynamic) { // modifies and returns self
  self->dynamic = dynamic;
  return self;
};

bool Var_isDynamic(Var *self) {
  bool retVal = self->dynamic;
  Ptr_release(self);
  return retVal;
};

bool Var_hasRoot(Var *self) {
  bool retVal =
      atomic_load_explicit(&self->root, memory_order_acquire) != RT_boxNull();
  Ptr_release(self);
  return retVal;
};

RTValue Var_deref(Var *self) {
  RTValue val = atomic_load_explicit(&self->root, memory_order_acquire);
  if (val != RT_boxNull()) {
    retain(val);
  }
  Ptr_release(self);
  return val;
};

RTValue Var_bindRoot(Var *self, RTValue object) {
  promoteToShared(object);
  RTValue oldRoot =
      atomic_exchange_explicit(&self->root, object, memory_order_seq_cst);
  atomic_fetch_add_explicit(&(self->rev), 1, memory_order_relaxed);

  if (oldRoot != RT_boxNull()) {
    autorelease(oldRoot);
  }

  Ptr_release(self);
  return RT_boxNil();
}

RTValue Var_unbindRoot(Var *self) {
  RTValue oldRoot =
      atomic_exchange_explicit(&self->root, RT_boxNull(), memory_order_seq_cst);
  atomic_fetch_add_explicit(&(self->rev), 1, memory_order_relaxed);

  if (oldRoot != RT_boxNull()) {
    autorelease(oldRoot);
  }

  Ptr_release(self);
  return RT_boxNil();
}

/* the returned reference is not retained and is not guaranteed to even
   be valid after the call returns. Outside ref system. */
RTValue Var_peek(Var *self) {
  return atomic_load_explicit(&self->root, memory_order_relaxed);
}

struct FunctionMethod *VarCallSlowPath(void *slot, RTValue currentVal,
                                       uint64_t argCount) {
  if (getType(currentVal) != functionType) {
    return NULL;
  }
  struct FunctionMethod *method = Function_extractMethod(currentVal, argCount);

  // IC Ownership: The IC slot owns a reference to the function (key).
  retain(currentVal);

  struct {
    RTValue key;
    void *method;
  } __attribute__((aligned(16))) pair;
  pair.key = currentVal;
  pair.method = method;

  typedef __int128_t int128;
  int128 *dest = (int128 *)slot;
  int128 src;
  memcpy(&src, &pair, 16);
  int128 old_val = __atomic_exchange_n(dest, src, __ATOMIC_ACQ_REL);

  struct {
    RTValue key;
    void *method;
  } *old_pair = (void *)&old_val;

  if (old_pair->key != 0) {
    release(old_pair->key);
  }
  return method;
}
