#include "Ebr.h"
#include "Object.h"
#include "RTValue.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

// Uncomment to enable detailed logging in EBR
// #define EBR_DEBUG

#ifdef EBR_DEBUG
#define EBR_LOG(fmt, ...)                                                      \
  do {                                                                         \
    struct timeval tv;                                                         \
    gettimeofday(&tv, NULL);                                                   \
    struct tm tm_buf;                                                          \
    struct tm *tm_info = localtime_r(&tv.tv_sec, &tm_buf);                     \
    char time_str[26];                                                         \
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);                 \
    fprintf(stderr, "[EBR %s.%06d] [Thread %p] " fmt "\n", time_str,           \
            (int)tv.tv_usec, (void *)pthread_self(), ##__VA_ARGS__);           \
  } while (0)
#else
#define EBR_LOG(fmt, ...) ((void)0)
#endif

// ---------------------------------------------------------
// OS-SPECIFIC ASYMMETRIC BARRIER MAGIC
// ---------------------------------------------------------

#if defined(__linux__)
#include <linux/membarrier.h>
#include <sys/syscall.h>
#include <unistd.h>

static void initialise() {
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

#define EBR_RECLAIM_THRESHOLD 32

typedef struct ThreadState {
  char padding[64]; // To avoid false sharing
  atomic_uint local_epoch;
  atomic_bool active;
  atomic_bool terminated;
  uword_t pending_count;
  _Atomic(struct ThreadState *) next;
} ThreadState;

typedef struct RetiredObject {
  RTValue value;
  unsigned int epoch_retired;
  struct RetiredObject *next;
} RetiredObject;

pthread_t reclaimer_thread_id;
pthread_mutex_t reclaimer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t synchronization_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t reclaimer_cond = PTHREAD_COND_INITIALIZER;
atomic_bool reclaimer_running = true;

atomic_uint global_epoch = 0;
_Atomic(ThreadState *) registry_head = NULL;
_Thread_local ThreadState *thread_state = NULL;
_Atomic(RetiredObject *) pending_reclamation = NULL;

_Atomic(size_t) active_threads = 0;

void *Ebr_reclaimer_thread_func(void *arg);

void Ebr_init() {
  EBR_LOG("Initializing EBR system.");
  initialise();
  atomic_store_explicit(&reclaimer_running, true, memory_order_release);
  pthread_create(&reclaimer_thread_id, NULL, Ebr_reclaimer_thread_func, NULL);
}

void Ebr_register_thread() {
  assert(!thread_state && "Thread already registered");
  atomic_fetch_add(&active_threads, 1);
  EBR_LOG("Registering thread...");
  // printf("After register we have %d threads\n", th + 1);
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
        curr->pending_count = 0;
        thread_state = curr;
        EBR_LOG("Reused terminated thread state %p.", (void *)curr);
        return;
      }
    }
    curr = atomic_load_explicit(&curr->next, memory_order_relaxed);
  }

  ThreadState *s = allocate(sizeof(ThreadState));
  EBR_LOG("Allocated new thread state %p.", (void *)s);
  atomic_init(&s->local_epoch, 0U);
  atomic_init(&s->active, false);
  atomic_init(&s->terminated, false);
  ThreadState *old_head;
  do {
    old_head = atomic_load_explicit(&registry_head, memory_order_relaxed);
    s->next = old_head;
  } while (!atomic_compare_exchange_weak_explicit(&registry_head, &old_head, s,
                                                  memory_order_release,
                                                  memory_order_relaxed));

  thread_state = s;
}

void Ebr_unregister_thread() {
  assert(thread_state && "Thread not registered");
  atomic_fetch_sub(&active_threads, 1);
  EBR_LOG("Unregistering thread (state %p).", (void *)thread_state);
  // printf("After unregister we have %d threads\n", xx - 1);
  atomic_store_explicit(&thread_state->active, false, memory_order_release);
  atomic_store_explicit(&thread_state->terminated, true, memory_order_release);
  thread_state = NULL;
}

void Ebr_force_reclaim() {
  // There should be only two threads left: the current thread and the reclaimer
  int wait_ms = 0;
  while (atomic_load(&active_threads) > 2) {
    usleep(1000);
    wait_ms++;
    if (wait_ms >= 1000 && (wait_ms % 1000 == 0)) {
      fprintf(stderr,
              "[EBR WARNING] Ebr_force_reclaim waiting: active_threads = %zu "
              "(> 2). Waited %d seconds.\n",
              (size_t)atomic_load(&active_threads), wait_ms / 1000);
    }
  }

  // Keep reclaiming until there are no more objects to reclaim.
  // We use the synchronization_mutex to properly wait for the background
  // reclaimer thread if it is currently holding the "stolen" pending list.
  int sync_wait_ms = 0;
  while (true) {
    Ebr_flush_critical();
    Ebr_synchronize_and_reclaim();

    pthread_mutex_lock(&synchronization_mutex);
    bool is_empty = (atomic_load_explicit(&pending_reclamation,
                                          memory_order_acquire) == NULL);
    pthread_mutex_unlock(&synchronization_mutex);

    if (is_empty) {
      break;
    }

    usleep(1000);
    sync_wait_ms++;
    if (sync_wait_ms >= 1000 && (sync_wait_ms % 1000 == 0)) {
      fprintf(stderr,
              "[EBR WARNING] Ebr_force_reclaim waiting to empty pending_reclamation. Waited %d seconds.\n",
              sync_wait_ms / 1000);
    }
  }
}

void Ebr_enter_critical() {
  unsigned int current =
      atomic_load_explicit(&global_epoch, memory_order_relaxed);
  EBR_LOG("Entering critical section (state %p, global_epoch: %u).",
          (void *)thread_state, current);
  atomic_store_explicit(&thread_state->active, true, memory_order_release);
  atomic_store_explicit(&thread_state->local_epoch, current,
                        memory_order_relaxed);
}

void Ebr_flush_critical() {
  unsigned int current =
      atomic_load_explicit(&global_epoch, memory_order_relaxed);
  EBR_LOG("Flushing critical section (state %p, global_epoch: %u).",
          (void *)thread_state, current);
  /* We need release barrier because we want to make sure all
     the reads completed before the barrier. */
  atomic_store_explicit(&thread_state->local_epoch, current,
                        memory_order_release);
}

void Ebr_leave_critical() {
  EBR_LOG("Leaving critical section (state %p).", (void *)thread_state);
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
  unsigned int current_ep =
      atomic_load_explicit(&global_epoch, memory_order_relaxed);
  node->epoch_retired = current_ep;

  RetiredObject *old_head;
  do {
    old_head = atomic_load_explicit(&pending_reclamation, memory_order_relaxed);
    node->next = old_head;
  } while (!atomic_compare_exchange_weak_explicit(
      &pending_reclamation, &old_head, node, memory_order_release,
      memory_order_relaxed));

  EBR_LOG("Object %p retired (epoch: %u, pending: %zu).", RT_unboxPtr(value),
          current_ep, thread_state->pending_count);
  if (thread_state->pending_count++ % EBR_RECLAIM_THRESHOLD == 0) {
    EBR_LOG("Reclaim threshold reached. Signaling reclaimer.");
    pthread_cond_signal(&reclaimer_cond);
  }
}

size_t Ebr_synchronize_and_reclaim() {
  pthread_mutex_lock(&synchronization_mutex);
  unsigned int current_global =
      atomic_load_explicit(&global_epoch, memory_order_relaxed);
#ifdef EBR_DEBUG
  EBR_LOG("Sync/Reclaim start (global_epoch: %u).", current_global);
#endif

  issue_global_barrier();

  bool can_advance = true;

  ThreadState *curr_thread =
      atomic_load_explicit(&registry_head, memory_order_acquire);
  while (curr_thread) {
    if (atomic_load_explicit(&curr_thread->active, memory_order_relaxed)) {
      unsigned int local_ep =
          atomic_load_explicit(&curr_thread->local_epoch, memory_order_relaxed);
      if (local_ep != current_global) {
        can_advance = false;
#ifdef EBR_DEBUG
        EBR_LOG(
            "Thread %p is active in epoch %u (global: %u). Blocking advance.",
            curr_thread, local_ep, current_global);
#endif
        break;
      }
    }
    curr_thread =
        atomic_load_explicit(&curr_thread->next, memory_order_relaxed);
  }

  if (can_advance) {
    atomic_fetch_add_explicit(&global_epoch, 1U, memory_order_release);
    current_global++;
    EBR_LOG("Advanced global_epoch to %u.", current_global);
  }

  RetiredObject *retiring = atomic_exchange_explicit(&pending_reclamation, NULL,
                                                     memory_order_acquire);

  RetiredObject *to_keep_head = NULL;
  RetiredObject *to_keep_tail = NULL;
  RetiredObject *cur = retiring;

  int reclaimed = 0;
#ifdef EBR_DEBUG
  int kept = 0;
#endif
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
#ifdef EBR_DEBUG
      kept++;
#endif
    }
    cur = next;
  }

  if (to_keep_head) {
    RetiredObject *old_head;
    do {
      old_head =
          atomic_load_explicit(&pending_reclamation, memory_order_relaxed);
      to_keep_tail->next = old_head;
    } while (!atomic_compare_exchange_weak_explicit(
        &pending_reclamation, &old_head, to_keep_head, memory_order_release,
        memory_order_relaxed));
  }

#ifdef EBR_DEBUG
  EBR_LOG("Sync/Reclaim done: Reclaimed %d, kept %d.", reclaimed, kept);
#endif

  pthread_mutex_unlock(&synchronization_mutex);
  return reclaimed;
}

void *Ebr_reclaimer_thread_func(void *arg) {
  Ebr_register_thread();
  EBR_LOG("Reclaimer thread started.");

  while (reclaimer_running) {
    pthread_mutex_lock(&reclaimer_mutex);
    pthread_cond_wait(&reclaimer_cond, &reclaimer_mutex);
    pthread_mutex_unlock(&reclaimer_mutex);

    EBR_LOG("Reclaimer thread woken up. Running synchronization...");
    Ebr_synchronize_and_reclaim();
  }
  EBR_LOG("Reclaimer thread exiting.");
  Ebr_unregister_thread();
  return NULL;
}

void Ebr_shutdown() {
  EBR_LOG("Shutting down EBR system.");
  pthread_mutex_lock(&reclaimer_mutex);
  atomic_store_explicit(&reclaimer_running, false, memory_order_release);
  pthread_cond_broadcast(&reclaimer_cond);
  pthread_mutex_unlock(&reclaimer_mutex);

  if (reclaimer_thread_id != 0) {
    pthread_join(reclaimer_thread_id, NULL);
  }

  RetiredObject *cur_garbage = atomic_exchange_explicit(
      &pending_reclamation, NULL, memory_order_acq_rel);

#ifdef EBR_DEBUG
  int final_reclaim = 0;
#endif
  while (cur_garbage) {
    RetiredObject *next = cur_garbage->next;
    release(cur_garbage->value);
    deallocate(cur_garbage);
    cur_garbage = next;
#ifdef EBR_DEBUG
    final_reclaim++;
#endif
  }
#ifdef EBR_DEBUG
  EBR_LOG("Shutdown: Reclaimed last %d pending objects.", final_reclaim);
#endif

  ThreadState *curr_thread =
      atomic_exchange_explicit(&registry_head, NULL, memory_order_acq_rel);
#ifdef EBR_DEBUG
  int thread_states_freed = 0;
#endif
  while (curr_thread) {
    ThreadState *next = curr_thread->next;
    deallocate(curr_thread);
    curr_thread = next;
#ifdef EBR_DEBUG
    thread_states_freed++;
#endif
  }
#ifdef EBR_DEBUG
  EBR_LOG("Shutdown: Freed %d thread state structures.", thread_states_freed);
#endif

  thread_state = NULL;
}
