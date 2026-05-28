#include "Var.h"
#include "Ebr.h"
#include "Exceptions.h"
#include "ExecutionContext.h"
#include "Namespace.h"
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

Var *Var_create(struct Symbol *sym) {
  assert(sym != NULL && "Must have sym to create a Var");
  Var *self = (Var *)allocate(sizeof(Var));
  atomic_store_explicit(&(self->root), RT_boxNull(), memory_order_relaxed);
  self->dynamic = false;
  self->ns = NULL;
  self->sym = sym;
  atomic_store_explicit(&self->metadata, RT_boxNil(), memory_order_relaxed);
  atomic_store_explicit(&self->rev, 0, memory_order_relaxed);
  Object_create((Object *)self, varType);
  // Var is always shared
  atomic_store_explicit(&(((Object *)self)->atomicRefCount),
                        COUNT_INC | SHARED_BIT, memory_order_release);
  return self;
};

Var *Var_create_interned(struct Namespace *ns, struct Symbol *sym) {
  Var *self = (Var *)allocate(sizeof(Var));
  atomic_store_explicit(&(self->root), RT_boxNull(), memory_order_relaxed);
  self->dynamic = false;
  self->ns = ns;
  Ptr_release(ns); // Consumes the refcount, but keeps it weak
  self->sym = sym;
  atomic_store_explicit(&self->metadata, RT_boxNil(), memory_order_relaxed);
  atomic_store_explicit(&self->rev, 0, memory_order_relaxed);
  Object_create((Object *)self, varType);
  atomic_store_explicit(&(((Object *)self)->atomicRefCount),
                        COUNT_INC | SHARED_BIT, memory_order_release);
  return self;
}

bool Var_equals(Var *self, Var *other) {
  return false; // pointer equality in Object_equals
};

uword_t Var_hash(Var *self) {
  /* Vars are purely pointer based entities */
  return hash(RT_boxPtr(self));
};

String *Var_toString(Var *self) {
  String *retVal = String_create("#'");
  Ptr_retain(self->sym);
  retVal =
      String_concat(retVal, Symbol_toString(RT_boxSymbol((Object *)self->sym)));
  Ptr_release(self);
  return retVal;
};

void Var_destroy(Var *self) {
  RTValue oldRoot = atomic_load_explicit(&self->root, memory_order_relaxed);
  if (oldRoot != RT_boxNull()) {
    autorelease(oldRoot);
  }
  Ptr_release(self->sym);
  RTValue oldMeta = atomic_load_explicit(&self->metadata, memory_order_relaxed);
  if (!RT_isNil(oldMeta)) {
    autorelease(oldMeta);
  }
};

Var *Var_resetMeta(Var *self, RTValue meta) {
  promoteToShared(meta);

  if (getType(meta) == persistentArrayMapType) {
    RTValue k = Keyword_create(String_create("dynamic"));
    Ptr_retain(RT_unboxPtr(meta));
    RTValue dynamicVal = PersistentArrayMap_get(RT_unboxPtr(meta), k);
    self->dynamic = RT_isTruthy(dynamicVal);
    release(dynamicVal);
  }

  RTValue oldMeta =
      atomic_exchange_explicit(&self->metadata, meta, memory_order_seq_cst);
  autorelease(oldMeta);
  return self;
}

RTValue Var_getMeta(Var *self) {
  RTValue val = atomic_load_explicit(&self->metadata, memory_order_relaxed);
  retain(val);
  Ptr_release(self);
  return val;
}

Var *Var_setDynamic(Var *self, bool dynamic) { // modifies and returns self
  self->dynamic = dynamic;
  return self;
};

// outside refcount system
bool Var_isDynamic(Var *self) { return self->dynamic; };

// outside refcount system
bool Var_hasRoot(Var *self) {
  return atomic_load_explicit(&self->root, memory_order_acquire) !=
         RT_boxNull();
};

/* context is borrowed! */
RTValue Var_deref(__attribute__((swift_context)) struct ExecutionContext *ctx,
                  Var *self) __attribute__((swiftcall)) {
  if (__builtin_expect(self->dynamic, 0) && ctx != NULL &&
      !RT_isNil(ctx->bindingsMap)) {
    assert(getType(ctx->bindingsMap) == persistentArrayMapType && "Wrong type");
    PersistentArrayMap *m = RT_unboxPtr(ctx->bindingsMap);
    RTValue key = RT_boxPtr(self);
    for (uword_t i = 0; i < m->count; i++) {
      // Pointer equality instead of "equals" for speed.
      if (key == m->keys[i]) {
        RTValue val = m->values[i];
        retain(val);
        Ptr_release(self);
        return val;
      }
    }
  }

  RTValue val = atomic_load_explicit(&self->root, memory_order_acquire);
  retain(val);
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

/* outside refcount system */
RTValue Var_peek(__attribute__((swift_context)) struct ExecutionContext *ctx,
                 Var *self) __attribute__((swiftcall)) {
  if (__builtin_expect(self->dynamic, 0) && ctx != NULL &&
      !RT_isNil(ctx->bindingsMap)) {
    assert(getType(ctx->bindingsMap) == persistentArrayMapType && "Wrong type");
    PersistentArrayMap *m = RT_unboxPtr(ctx->bindingsMap);
    RTValue key = RT_boxPtr(self);
    for (uword_t i = 0; i < m->count; i++) {
      // Pointer equality instead of "equals" for speed.
      if (key == m->keys[i]) {
        return m->values[i];
      }
    }
  }
  return atomic_load_explicit(&self->root, memory_order_acquire);
}

RTValue Var_set(__attribute__((swift_context)) struct ExecutionContext *ctx,
                Var *self, RTValue value) __attribute__((swiftcall)) {
  if (__builtin_expect(!self->dynamic, 0)) {
    release(value);
    Ptr_release(self);
    throwIllegalStateException_C("Can't set! a non-dynamic Var");
  }

  if (!ctx || RT_isNil(ctx->bindingsMap)) {
    release(value);
    Ptr_release(self);
    throwIllegalStateException_C(
        "Can't set! dynamic Var outside of a binding context");
  }

  // Check if the var has a thread-local binding (Clojure behavior)
  PersistentArrayMap *m = RT_unboxPtr(ctx->bindingsMap);
  Ptr_retain(m); // indexOf consumes map
  Ptr_retain(self); // indexOf consumes key
  if (PersistentArrayMap_indexOf(m, RT_boxPtr(self)) == -1) {
    release(value);
    Ptr_release(self);
    throwIllegalStateException_C("Can't set!: Var has no thread-local binding");
  }

  retain(value);
  RTValue oldMap = ctx->bindingsMap;
  Ptr_retain(self); // assoc consumes key
  Ptr_retain(m); // assoc consumes map
  ctx->bindingsMap =
      RT_boxPtr(PersistentArrayMap_assoc(m, RT_boxPtr(self), value));
  release(oldMap);

  Ptr_release(self);

  return value;
}
