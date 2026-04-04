#include "Ebr.h"
#include "Object.h"
#include "RTValue.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>

// ---------------------------------------------------------
// OS-SPECIFIC ASYMMETRIC BARRIER MAGIC
// ---------------------------------------------------------

#if defined(__linux__)
#include <linux/membarrier.h>
#include <sys/syscall.h>
#include <unistd.h>

static void intialise() {
  // expedited private membarrier
  int ret =
      syscall(__NR_membarrier, MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED, 0);
  if (ret < 0) {
    perror("Linux membarrier register failed. Update your kernel.");
    exit(1);
  }
}

static void issue_global_barrier() {
  syscall(__NR_membarrier, MEMBARRIER_CMD_PRIVATE_EXPEDITED, 0);
}

#elif defined(__APPLE__)
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

static void *dummy_page = NULL;
static size_t page_size = 0;

static void initialise() {
  page_size = sysconf(_SC_PAGESIZE);
  dummy_page = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANON, -1, 0);
  if (dummy_page == MAP_FAILED) {
    fprintf(stderr, "macOS mmap failed for dummy page\n");
    exit(1);
  }
}

static void issue_global_barrier() {
  // TLB Shootdown Hack
  mprotect(dummy_page, page_size, PROT_READ);
  mprotect(dummy_page, page_size, PROT_READ | PROT_WRITE);
}

#else
#error "Unsupported Operating System for Asymmetric Barriers"
#endif

// ------------------------ END ----------------------------
// OS-SPECIFIC ASYMMETRIC BARRIER MAGIC
// ------------------------ END ----------------------------

#define EBR_RECLAIM_THRESHOLD 1024

typedef struct ThreadState {
  char padding[64]; // To avoid false sharing
  atomic_uint local_epoch;
  atomic_bool active;
  atomic_bool terminated;
  _Atomic(struct ThreadState *) next;
} ThreadState;

typedef struct RetiredObject {
  RTValue value;
  unsigned int epoch_retired;
  struct RetiredObject *next;
} RetiredObject;

pthread_t reclaimer_thread_id;
pthread_mutex_t reclaimer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t reclaimer_cond = PTHREAD_COND_INITIALIZER;
atomic_bool reclaimer_running = true;

atomic_size_t pending_count = ATOMIC_VAR_INIT(0);

atomic_uint global_epoch = ATOMIC_VAR_INIT(0);
_Atomic(ThreadState *) registry_head = NULL;
_Thread_local ThreadState *thread_state = NULL;
_Atomic(RetiredObject *) pending_reclamation = NULL;


void *Ebr_reclaimer_thread_func(void *arg);

void Ebr_init() {
  initialise();
  atomic_store_explicit(&reclaimer_running, true, memory_order_seq_cst);
  pthread_create(&reclaimer_thread_id, NULL, Ebr_reclaimer_thread_func, NULL);
}

void Ebr_register_thread() {
  if (thread_state)
    return;
  ThreadState *curr =
      atomic_load_explicit(&registry_head, memory_order_acquire);
  while (curr) {
    bool expected = true;
    if (atomic_load_explicit(&curr->terminated, memory_order_relaxed)) {
      if (atomic_compare_exchange_strong_explicit(&curr->terminated, &expected,
                                                  false, memory_order_acq_rel,
                                                  memory_order_relaxed)) {

        atomic_store_explicit(&curr->active, false, memory_order_release);
        atomic_store_explicit(&curr->local_epoch, 0U, memory_order_relaxed);
        thread_state = curr;
        return;
      }
    }
    curr = atomic_load_explicit(&curr->next, memory_order_relaxed);
  }

  ThreadState *s = allocate(sizeof(ThreadState));
  atomic_init(&s->local_epoch, 0U);
  atomic_init(&s->active, false);
  atomic_init(&s->terminated, false);
  ThreadState *old_head;
  do {
    old_head = atomic_load(&registry_head);
    s->next = old_head;
  } while (!atomic_compare_exchange_weak(&registry_head, &old_head, s));

  thread_state = s;
}

void Ebr_unregister_thread() {
  if (!thread_state)
    return;
  atomic_store_explicit(&thread_state->active, false, memory_order_release);
  atomic_store_explicit(&thread_state->terminated, true, memory_order_release);
  thread_state = NULL;
}

void Ebr_enter_critical() {
  atomic_store_explicit(&thread_state->active, true, memory_order_release);
  unsigned int current =
      atomic_load_explicit(&global_epoch, memory_order_acquire);
  atomic_store_explicit(&thread_state->local_epoch, current,
                        memory_order_relaxed);
}

void Ebr_leave_critical() {
  atomic_signal_fence(memory_order_seq_cst);
  atomic_store_explicit(&thread_state->active, false, memory_order_release);
}

void autorelease(RTValue value) {
  if (!RT_isPtr(value)) {
    return;
  }
  // TODO: It is important to later on make sure we use a custom allocator with
  // a dedicated bank so that we dont do malloc/free here.
  RetiredObject *node = allocate(sizeof(RetiredObject));
  node->value = value;
  node->epoch_retired =
      atomic_load_explicit(&global_epoch, memory_order_relaxed);

  RetiredObject *old_head;
  do {
    old_head = atomic_load(&pending_reclamation);
    node->next = old_head;
  } while (
      !atomic_compare_exchange_weak(&pending_reclamation, &old_head, node));

  if (atomic_fetch_add(&pending_count, 1) >= EBR_RECLAIM_THRESHOLD) {
    pthread_cond_signal(&reclaimer_cond);
  }
}

size_t Ebr_synchronize_and_reclaim() {
  issue_global_barrier();

  unsigned int current_global =
      atomic_load_explicit(&global_epoch, memory_order_acquire);
  bool can_advance = true;

  ThreadState *curr_thread =
      atomic_load_explicit(&registry_head, memory_order_acquire);
  while (curr_thread) {
    if (atomic_load_explicit(&curr_thread->active, memory_order_acquire)) {
      unsigned int local_ep =
          atomic_load_explicit(&curr_thread->local_epoch, memory_order_acquire);
      if (local_ep != current_global) {
        can_advance = false;
        break;
      }
    }
    curr_thread =
        atomic_load_explicit(&curr_thread->next, memory_order_relaxed);
  }

  if (can_advance) {
    atomic_fetch_add_explicit(&global_epoch, 1U, memory_order_release);
    current_global++;
  }

  RetiredObject *retiring = atomic_exchange(&pending_reclamation, NULL);

  RetiredObject *to_keep_head = NULL;
  RetiredObject *to_keep_tail = NULL;
  RetiredObject *cur = retiring;

  int reclaimed = 0;
  while (cur) {
    RetiredObject *next = cur->next;

    if (current_global - cur->epoch_retired >= 2) {
      release(cur->value);
      deallocate(cur);
      reclaimed++;
    } else {
      cur->next = NULL;
      if (!to_keep_head) {
        to_keep_head = cur;
        to_keep_tail = cur;
      } else {
        to_keep_tail->next = cur;
        to_keep_tail = cur;
      }
    }
    cur = next;
  }

  if (to_keep_head) {
    RetiredObject *old_head;
    do {
      old_head = atomic_load(&pending_reclamation);
      to_keep_tail->next = old_head;
    } while (!atomic_compare_exchange_weak(&pending_reclamation, &old_head,
                                           to_keep_head));
  }

  return reclaimed;
}

void *Ebr_reclaimer_thread_func(void *arg) {
  Ebr_register_thread();

  while (reclaimer_running) {
    pthread_mutex_lock(&reclaimer_mutex);

    while (atomic_load(&pending_count) < EBR_RECLAIM_THRESHOLD &&
           reclaimer_running) {
      pthread_cond_wait(&reclaimer_cond, &reclaimer_mutex);
    }

    pthread_mutex_unlock(&reclaimer_mutex);

    if (!reclaimer_running)
      break;

    size_t reclaimed = Ebr_synchronize_and_reclaim();

    atomic_fetch_sub(&pending_count, reclaimed);
  }

  return NULL;
}

void Ebr_shutdown() {
  pthread_mutex_lock(&reclaimer_mutex);
  atomic_store_explicit(&reclaimer_running, false, memory_order_release);
  pthread_cond_broadcast(&reclaimer_cond);
  pthread_mutex_unlock(&reclaimer_mutex);

  if (reclaimer_thread_id != 0) {
    pthread_join(reclaimer_thread_id, NULL);
  }

  RetiredObject *cur_garbage = atomic_exchange_explicit(
      &pending_reclamation, NULL, memory_order_acq_rel);

  while (cur_garbage) {
    RetiredObject *next = cur_garbage->next;
    release(cur_garbage->value);
    deallocate(cur_garbage);
    cur_garbage = next;
  }

  ThreadState *curr_thread =
      atomic_exchange_explicit(&registry_head, NULL, memory_order_acq_rel);
  while (curr_thread) {
    ThreadState *next = curr_thread->next;
    free(curr_thread);
    curr_thread = next;
  }

  thread_state = NULL;
}

