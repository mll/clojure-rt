#include "Var.h"
#include "Object.h"
#include "RTValue.h"
#include "String.h"
#include "word.h"
#include <sys/mman.h>
#include <unistd.h>

// TODO: UnboundClass is printed in different way
// Class *UNIQUE_UnboundClass;

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

typedef struct HazardSlot {
  _Atomic(RTValue) hazardPointer;
  _Atomic(bool) active;
  struct HazardSlot *next;
} HazardSlot;

_Atomic(HazardSlot *) hazardHead = NULL;
static __thread HazardSlot *threadLocalHazardSlot = NULL;
static pthread_key_t cleanup_gatekeeper;

static void *dummy_page = NULL;
void asymmetric_barrier() {
  if (!dummy_page) {
    dummy_page = mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  }
  // Toggle protection to force TLB shootdown / System-wide barrier on all cores
  mprotect(dummy_page, getpagesize(), PROT_READ);
  mprotect(dummy_page, getpagesize(), PROT_READ | PROT_WRITE);
}

void on_thread_exit(void *unused) {
  if (threadLocalHazardSlot != NULL) {
    bool expectedValue = true;
    atomic_compare_exchange_weak(&(threadLocalHazardSlot->active),
                                 &expectedValue, false);
  }
}

void Var_initialize() {
  pthread_key_create(&cleanup_gatekeeper, on_thread_exit);
}

void Var_thread_initialize() {
  pthread_setspecific(cleanup_gatekeeper, (void *)0x1);
}

void Var_cleanup() { /* to be run when all other threads are dead */
  HazardSlot *current_head;
  while ((current_head = atomic_load(&hazardHead)) != NULL) {
    HazardSlot *next_head = current_head->next;
    if (!atomic_compare_exchange_weak(&hazardHead, &current_head, next_head)) {
      continue;
    }
    free(current_head);
  }
}

HazardSlot *getOrCreateSlot() {
  if (threadLocalHazardSlot)
    return threadLocalHazardSlot;

  // 1. Try to find an inactive slot to recycle
  HazardSlot *curr = atomic_load(&hazardHead);
  while (curr) {
    bool expected = false;
    if (atomic_compare_exchange_strong(&curr->active, &expected, true)) {
      threadLocalHazardSlot = curr;
      return curr;
    }
    curr = curr->next;
  }

  // 2. No free slots, allocate new
  HazardSlot *new_node = calloc(1, sizeof(HazardSlot));
  new_node->active = true;

  // Lock-free push to the global list
  HazardSlot *old_head;
  do {
    old_head = atomic_load(&hazardHead);
    new_node->next = old_head;
  } while (!atomic_compare_exchange_weak(&hazardHead, &old_head, new_node));

  threadLocalHazardSlot = new_node;
  return threadLocalHazardSlot;
}

Var *Var_create(RTValue keyword) {
  Var *self = (Var *)allocate(sizeof(Var));
  //  retain(UNIQUE_UnboundClass);
  atomic_init(&(self->root),
              RT_boxNull()); //(Object *)Deftype_create(UNIQUE_UnboundClass, 1,
                             // keyword);
  self->dynamic = false;
  self->keyword = keyword;
  atomic_init(&self->rev, 0);
  Object_create((Object *)self, varType);
  return self;
};

bool Var_equals(Var *self, Var *other) {
  return false; // pointer equality in Object_equals
};

uword_t Var_hash(Var *self) {
  return combineHash(hash(self->keyword), hash(self->root));
};

String *Var_toString(Var *self) {
  String *retVal = String_create("#");
  retVal = String_concat(retVal, String_replace(toString(self->keyword),
                                                String_create(":"),
                                                String_create("'")));
  Ptr_release(self);
  return retVal;
};

void Var_destroy(Var *self) {
  RTValue oldRoot = self->root;
  if (oldRoot != RT_boxNull()) {
    asymmetric_barrier();
    HazardSlot *newHead = atomic_load(&hazardHead);
    HazardSlot *curr;
    HazardSlot *head;

    do {
      head = newHead;
      curr = head;
      while (curr) {
        while (atomic_load_explicit(&curr->hazardPointer,
                                    memory_order_seq_cst) == oldRoot) {
          CPU_PAUSE();
        }
        curr = curr->next;
      }
      newHead = atomic_load(&hazardHead);
    } while (newHead != head);
  }

  release(oldRoot);
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
  HazardSlot *slot = getOrCreateSlot();

  while (true) {
    RTValue val = atomic_load_explicit(&self->root, memory_order_acquire);
    if (val == RT_boxNull()) {
      Ptr_release(self);
      return val;
    }
    // Stage 1: Advertise
    // Relaxed store: writer will force visibility with asymmetric_barrier()
    atomic_store_explicit(&slot->hazardPointer, val, memory_order_relaxed);

    // Prevent compiler from reordering the load of root before the slot store
    atomic_signal_fence(memory_order_seq_cst);

    // Stage 2: Verify (The "Double Check")
    // Acquire load is enough here because writer ensures its update is visible
    // before we’d potentially use the old freed value.
    if (val == atomic_load_explicit(&self->root, memory_order_acquire)) {
      retain(val);
      atomic_store_explicit(&slot->hazardPointer, RT_boxNull(),
                            memory_order_relaxed);
      Ptr_release(self);
      return val;
    }

    // Global changed, retry
    atomic_store_explicit(&slot->hazardPointer, RT_boxNull(),
                          memory_order_relaxed);
  }
  // TODO: threadBound
};

RTValue Var_bindRoot(Var *self, RTValue object) {
  // An optimisation can be made here - instead of immediately scanning hazards,
  // the pointer can be added to a retire list which is scanned once every
  // N operations. This trades predictability for performance.

  promoteToShared(object);
  RTValue oldRoot =
      atomic_exchange_explicit(&self->root, object, memory_order_seq_cst);
  atomic_fetch_add_explicit(&(self->rev), 1, memory_order_relaxed);

  if (oldRoot != RT_boxNull()) {
    // Heavy synchronization: ensure all readers see the update
    // and writer sees all reader advertisements.
    asymmetric_barrier();

    HazardSlot *newHead = atomic_load(&hazardHead);
    HazardSlot *curr;
    HazardSlot *head;

    do {
      head = newHead;
      curr = head;
      while (curr) {
        while (atomic_load_explicit(&curr->hazardPointer,
                                    memory_order_seq_cst) == oldRoot) {
          CPU_PAUSE();
        }
        curr = curr->next;
      }
      newHead = atomic_load(&hazardHead);
    } while (newHead != head);
  }

  release(oldRoot);
  Ptr_release(self);
  return RT_boxNil();
}

RTValue Var_unbindRoot(Var *self) {
  RTValue oldRoot =
      atomic_exchange_explicit(&self->root, RT_boxNull(), memory_order_seq_cst);
  atomic_fetch_add_explicit(&(self->rev), 1, memory_order_relaxed);

  if (oldRoot != RT_boxNull()) {
    asymmetric_barrier();
    HazardSlot *newHead = atomic_load(&hazardHead);
    HazardSlot *curr;
    HazardSlot *head;

    do {
      head = newHead;
      curr = head;
      while (curr) {
        while (atomic_load_explicit(&curr->hazardPointer,
                                    memory_order_seq_cst) == oldRoot) {
          CPU_PAUSE();
        }
        curr = curr->next;
      }
      newHead = atomic_load(&hazardHead);
    } while (newHead != head);
  }

  release(oldRoot);
  Ptr_release(self);
  return RT_boxNil();
}

/* the returned reference is not retained and is not guaranteed to even
   be valid after the call returns. */
RTValue Var_peek(Var *self) {
  RTValue val = atomic_load_explicit(&self->root, memory_order_relaxed);
  Ptr_release(self);
  return val;
}
