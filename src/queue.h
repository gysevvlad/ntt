#pragma once

#include "ntt/loop.h"
#include "ntt/queue.h" // IWYU pragma: export

#include "./queue_heap.h"
#include "./slab.h"
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
    /** First member: the slab owns it. */
    ntt_slab_head_t slab_head;

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

    /** Generation this queue last registered with. Queue context only. */
    uint32_t timer_reg_gen;

    /** Deadline this queue last registered. Queue context only. */
    uint64_t timer_reg_deadline;

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
 * @brief The generation of the registration this queue would use now.
 */
static inline uint32_t ntt_queue_timer_generation(const ntt_queue_t* self)
{
    return ntt_slab_generation(self);
}

/**
 * @brief Bump the registration generation, retiring every entry of this queue.
 *
 * @pre Called from the context of this queue.
 */
static inline uint32_t ntt_queue_retire_timer_generation(ntt_queue_t* self)
{
    return ntt_slab_retire(self);
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
 * @note Only for assert check. May be called while another thread pushes.
 */
int ntt_queue_empty(ntt_queue_t* queue);

/**
 * @brief Take pointer to the first task in queue.
 *
 * May be called while another thread pushes.
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
 * @brief Whether this queue came from the slab of a loop.
 *
 * Only a queue of a loop comes from a slab; the local queue of a worker and
 * the ones tests build by hand do not, and their slab head is not set up.
 */
static inline int ntt_queue_from_slab(const ntt_queue_t* self)
{
    return self->loop != NULL ? 1 : 0;
}

/**
 * @brief Push task to the queue.
 *
 * Whatever the caller wrote into the task before the push is visible to the
 * context that takes it out of the queue.
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
