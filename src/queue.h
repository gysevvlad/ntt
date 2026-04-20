#pragma once

#include "ntt/loop.h"
#include "ntt/queue.h" // IWYU pragma: export

#include "./task_list.h"
#include "ntt/impl/atomic.h"
#include "ntt/task.h"

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
 * @note Only for assert check.
 */
int ntt_queue_empty(ntt_queue_t* queue);

/**
 * @brief Take pointer to the first task in queue.
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
 * @brief Push task to the queue.
 *
 * @pre ntt_task_unlinked(task) == 1
 */
void ntt_queue_push_task(ntt_queue_t* queue, ntt_task_t* task);

/**
 * @brief Push task to the queue without invoking wakeup callback.
 *
 * Adds a task to the queue internal list without triggering the wakeup callback.
 * This is useful when wakeup is processed by the caller side.
 *
 * @param[in] queue The target queue.
 * @param[in] task The task to push.
 *
 * @return Returns 1 if the queue was empty before the push (meaning queue must be
 *         executed), 0 otherwise.
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
 * @pre ntt_queue_empty(qeueu) == 0
 * @pre ntt_queue_unlinked(queue) == 1
 */
size_t ntt_queue_svc_with_limit(ntt_queue_t* queue, size_t limit);

/**
 * @brief Execute tasks in queue.
 *
 * @param[in] queue The queue from which events will be executed.
 *
 * @pre ntt_queue_empty(self) == 0
 * @pre ntt_queue_unlinked(self) == 1
 */
void ntt_queue_svc(ntt_queue_t* self);

EXTERN_STOP
