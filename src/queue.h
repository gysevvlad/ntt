#pragma once

#include "ntt/loop.h"
#include "ntt/queue.h" // IWYU pragma: export

#include "./queue_heap.h"
#include "./queue_slab.h"
#include "./task_list.h"
#include "./timer_heap.h"
#include "./timer_service.h"
#include "ntt/impl/atomic.h"
#include "ntt/task.h"
#include "ntt/timer.h"

EXTERN_START

#include <pthread.h>

typedef void(ntt_queue_wakeup_cb_t)(void* context, ntt_queue_t* queue);

struct ntt_queue {
    atomic_size_t external_refs;
    ntt_task_list_t task_list;
    pthread_spinlock_t lock;
    ntt_node_t node;
    void* context;
    ntt_queue_wakeup_cb_t* wakeup_cb;
    ntt_loop_t* loop;

    /** Timers of this queue. Touched only from its own serial context. */
    ntt_timer_heap_t timers;

    ntt_timer_service_t* timer_service;

    /**
     * @brief Generation of the registration in the timer service.
     *
     * The single source of truth about whether an entry of the service still
     * stands. Whoever invalidates a registration bumps it: the queue context
     * when it registers again or has no timer left, and a worker when it takes
     * the entry out to wake the queue. An entry carrying an older value is a
     * tombstone, so dropping a registration touches neither the heap of the
     * service nor its lock.
     *
     * Owned by the slab across reuse of the slot, so an entry of a dead queue
     * can never match a later tenant.
     */
    atomic_uint_fast32_t timer_gen;

    /** Generation this queue last registered with. Queue context only. */
    uint32_t timer_reg_gen;

    /** Deadline this queue last registered. Queue context only. */
    uint64_t timer_reg_deadline;

    /** Slot in the queue slab, NTT_QUEUE_SLAB_NONE when not from the slab. */
    uint32_t slab_index;

    /** Free list link of the slab, owned by it, meaningless while alive. */
    uint32_t slab_next_free;

    /** Whether the queue holds a reference to itself for its armed timers. */
    int timer_self_ref;

    /**
     * @brief Entries of this queue that are tombstones but still in the heap.
     *
     * Retiring a registration does not reach the service, so the tombstone it
     * leaves is counted here and handed over with the next registration. The
     * service needs the count to know when to rebuild its heap.
     */
    uint32_t timer_tombstones;
};

/**
 * @brief Initialize ntt_queue_t.
 */
void ntt_queue_init(
    ntt_queue_t* self,
    void* context,
    ntt_queue_wakeup_cb_t* wakeup_cb,
    ntt_loop_t* loop);

/**
 * @brief Slab bookkeeping, used only by ntt_queue_slab.
 */
static inline uint32_t ntt_queue_slab_next_free(const ntt_queue_t* self)
{
    return self->slab_next_free;
}

static inline void ntt_queue_slab_set_next_free(ntt_queue_t* self, uint32_t next)
{
    self->slab_next_free = next;
}

static inline void ntt_queue_slab_init_generation(ntt_queue_t* self)
{
    atomic_store_explicit(&self->timer_gen, 1, memory_order_relaxed);
}

static inline void ntt_queue_slab_bump_generation(ntt_queue_t* self)
{
    atomic_fetch_add_explicit(&self->timer_gen, 1, memory_order_release);
}

/**
 * @brief The generation of the registration this queue would use now.
 */
static inline uint32_t ntt_queue_timer_generation(const ntt_queue_t* self)
{
    return (uint32_t)atomic_load_explicit(&self->timer_gen, memory_order_acquire);
}

/**
 * @brief Bump the registration generation, retiring every entry of this queue.
 *
 * @pre Called from the context of this queue.
 */
static inline uint32_t ntt_queue_retire_timer_generation(ntt_queue_t* self)
{
    return (uint32_t)atomic_fetch_add_explicit(&self->timer_gen, 1, memory_order_release) + 1;
}

/**
 * @brief Take a reference only if the queue is still alive.
 *
 * Needed by the timer service: the storage of a queue outlives it, so an entry
 * may be looked at after the queue is gone. Acquiring without this check could
 * resurrect a queue that is being destroyed.
 *
 * @return Returns 1 when a reference was taken, otherwise 0.
 */
static inline int ntt_queue_acquire_if_live(ntt_queue_t* self)
{
    size_t refs = atomic_load_explicit(&self->external_refs, memory_order_relaxed);

    for (;;) {
        if (refs == 0) {
            return 0;
        }

        if (atomic_compare_exchange_weak_explicit(
                &self->external_refs,
                &refs,
                refs + 1,
                memory_order_acquire,
                memory_order_relaxed)) {
            return 1;
        }
    }
}

/**
 * @brief Get control block counter.
 */
static inline size_t ntt_queue_use_count(
    ntt_queue_t* self)
{
    return atomic_load_explicit(&self->external_refs, memory_order_relaxed);
}

/**
 * @brief Deinit ntt queue.
 *
 * Does not give back the work of the loop that the queue holds: that has to
 * happen after the storage of the queue is handed back, because the loop may
 * finish and release that storage as soon as it does.
 *
 * @pre ntt_queue_use_count(self) == 0
 * @pre ntt_queue_empty(self) == 1
 * @pre ntt_queue_unlinked(self) == 1
 */
void ntt_queue_deinit(
    ntt_queue_t* self);

/**
 * @brief Debug check that queue not linked with loop queue.
 *
 * @return Returns 1 if queue not linked, and 0 if linked.
 *
 * @note Only for assert check.
 *
 * @pre queue != NULL
 */
int ntt_queue_unlinked(ntt_queue_t* queue);

/**
 * @brief The function checks that the queue does not contain tasks.
 *
 * @return Returns 1 if the queue is empty, 0 otherwise.
 *
 * @note Only for assert check. Takes the lock of the queue, so in a release
 *       build, where the asserts are gone, it costs nothing.
 */
int ntt_queue_empty(ntt_queue_t* queue);

/**
 * @brief Take pointer to the first task in queue.
 *
 * Also the point where this queue synchronizes with whoever pushed the task:
 * the lock taken here pairs with the unlock of the push.
 *
 * @pre ntt_queue_empty(queue) == 0
 */
ntt_task_t* ntt_queue_front(ntt_queue_t* queue);

/**
 * @brief Forgot first task from queue and return pointer to the next task.
 *
 * @return Returns pointer to the next task or NULL if no more tasks.
 *
 * @pre ntt_queue_unlinked(queue) == 1
 * @pre ntt_queue_empty(queue) == 0
 */
ntt_task_t* ntt_queue_next(ntt_queue_t* queue);

/**
 * @brief Push the task that drains the timers of this queue.
 *
 * Called by the timer service when the deadline of the queue has passed. The
 * task runs the expired timers in the queue context, one at a time, so that
 * the callback of one may still cancel another.
 */
void ntt_queue_wakeup_timers(ntt_queue_t* queue);

/**
 * @brief Bind the queue to a timer service.
 */
void ntt_queue_set_timer_service(ntt_queue_t* queue, ntt_timer_service_t* service);

/**
 * @brief The timer service of this queue, NULL when it has none.
 */
static inline ntt_timer_service_t* ntt_queue_timer_service(ntt_queue_t* queue)
{
    return queue->timer_service;
}

/**
 * @brief The timer heap of this queue.
 *
 * @pre Called from the context of this queue.
 */
static inline ntt_timer_heap_t* ntt_queue_timers(ntt_queue_t* queue)
{
    return &queue->timers;
}

/**
 * @brief Register the earliest deadline of this queue with its service.
 *
 * Does nothing when the service already knows a deadline that is not later.
 * Drops the registration when no timer is left, which costs one atomic add.
 *
 * @pre Called from the context of this queue.
 */
void ntt_queue_sync_timers(ntt_queue_t* queue);

/**
 * @brief Create a queue of this loop, taken from its queue slab.
 */
ntt_queue_t* ntt_queue_create_in(ntt_loop_t* loop);

/**
 * @brief The slot of this queue in the slab.
 */
static inline uint32_t ntt_queue_slab_index(const ntt_queue_t* self)
{
    return self->slab_index;
}

/**
 * @brief Push task to the queue.
 *
 * @note Acquires the queue if it was empty before the push.
 *
 * @pre ntt_task_unlinked(task) == 1
 */
void ntt_queue_push_task(ntt_queue_t* queue, ntt_task_t* task);

/**
 * @brief Push task to the queue without invoking wakeup callback.
 *
 * Adds a task to the queue internal list without triggering the wakeup callback.
 * This is useful when wakeup is processed by the caller side (e.g., after batch
 * push operations). The queue may be linked or unlinked.
 *
 * @note The function never acquire the queue.
 *
 * @param[in] queue The target queue.
 * @param[in] task The task to push.
 *
 * @return Returns 1 if the queue was empty before the push (meaning queue must be
 *         executed by the caller side), 0 otherwise.
 *
 * @pre ntt_task_unlinked(task) == 1
 */
int ntt_queue_push_task_without_wakeup(ntt_queue_t* queue, ntt_task_t* task);

/**
 * @brief Execute tasks in queue.
 *
 * @param[in] queue The queue from which events will be executed.
 * @param[in] limit Maximum number of tasks to complete.
 *
 * @return Returns 0 if the limit of tasks have been completed
 *         and there are still tasks left in the queue.
 *         Otherwise, returns the number of completed tasks.
 *
 * @note Releases the queue only if returns non-zero (all tasks completed).
 *       The self pointer may be deleted after return.
 *
 * @pre ntt_queue_empty(qeueu) == 0
 * @pre ntt_queue_unlinked(queue) == 1
 */
size_t ntt_queue_svc_with_limit(ntt_queue_t* queue, size_t limit);

/**
 * @brief Execute tasks in queue.
 *
 * @param[in] self The queue from which events will be executed.
 *
 * @note Always releases the queue. The self pointer may be deleted after return.
 *
 * @pre ntt_queue_empty(self) == 0
 * @pre ntt_queue_unlinked(self) == 1
 */
void ntt_queue_svc(ntt_queue_t* self);

EXTERN_STOP
