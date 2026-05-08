#include "Var.h"
#include "Ebr.h"
#include "Exceptions.h"
#include "ExecutionContext.h"
#include "Object.h"
#include "RTValue.h"
#include "Namespace.h"
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
  self->ns = NULL;
  self->sym = NULL;
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
  if(ns) Ptr_retain(ns);
  self->sym = sym;
  if(sym) Ptr_retain(sym);
  
  
  Ptr_retain(ns->name);
  String *nsStr = Symbol_toString(RT_boxSymbol((Object *)ns->name));
  String *slashStr = String_create("/");
  Ptr_retain(sym);
  String *symStr = Symbol_toString(RT_boxSymbol((Object *)sym));

  
  String *s1 = String_concat(nsStr, slashStr);
  String *s2 = String_concat(s1, symStr);
  
  self->keyword = Keyword_create(s2); // keyword takes ownership
  
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
  RTValue oldMeta = atomic_load_explicit(&self->metadata, memory_order_relaxed);
  if (!RT_isNil(oldMeta)) {
    autorelease(oldMeta);
  }
  if (self->ns) {
    Ptr_release(self->ns);
  }
  if (self->sym) {
    Ptr_release(self->sym);
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
    RTValue key = self->keyword;
    for (uword_t i = 0; i < m->count; i++) {
      if (equals(key, m->keys[i])) {
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
    RTValue key = self->keyword;
    for (uword_t i = 0; i < m->count; i++) {
      if (equals(key, m->keys[i])) {
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
  Ptr_retain(m);
  retain(self->keyword);
  if (PersistentArrayMap_indexOf(m, self->keyword) == -1) {
    release(value);
    Ptr_release(self);
    throwIllegalStateException_C("Can't set!: Var has no thread-local binding");
  }

  retain(self->keyword);
  retain(value);
  ctx->bindingsMap = RT_boxPtr(PersistentArrayMap_assoc(
      RT_unboxPtr(ctx->bindingsMap), self->keyword, value));

  Ptr_release(self);

  return value;
}
