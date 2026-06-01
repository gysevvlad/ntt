#pragma once

#include "./queue.h"

EXTERN_START

#include <pthread.h>

/**
 * @brief Combine queue - a queue of queues.
 *
 * Combines multiple ntt_queue_t into a single queue.
 * Invokes wakeup callback when transition from empty to non-empty.
 */
typedef struct ntt_combine ntt_combine_t;

/**
 * @brief Wakeup callback for combine queue.
 *
 * Invoked when combine transitions from empty to non-empty.
 *
 * @param[in] context User context from ntt_combine_init_with_context.
 * @param[in] queue The combine queue that became non-empty.
 */
typedef void(ntt_combine_wakeup_cb_t)(void* context, ntt_combine_t* queue);

struct ntt_combine {
    pthread_spinlock_t lock;
    ntt_list_t list;
    void* ctx;
    ntt_combine_wakeup_cb_t* cb;
};

/**
 * @brief Init ntt loop queue.
 */
void ntt_combine_init(ntt_combine_t* self);

/**
 * @brief Initialize combine queue with wakeup callback.
 *
 * @param[in] self The combine queue.
 * @param[in] ctx User context passed to wakeup callback.
 * @param[in] cb Wakeup callback invoked when queue becomes non-empty.
 */
void ntt_combine_init_with_wakeup(
    ntt_combine_t* self,
    void* ctx,
    ntt_combine_wakeup_cb_t* cb);

/**
 * @brief Check that loop queue is empty.
 */
int ntt_combine_empty(ntt_combine_t* self);

/**
 * @brief Deinit ntt loop queue.
 *
 * @pre ntt_combine_empty()
 */
void ntt_combine_deinit(ntt_combine_t* self);

/**
 * @brief Push queue to the combine.
 *
 * @note Invokes wakeup callback if combine was empty before the push.
 *
 * @pre ntt_queue_unlinked(queue) == 1
 */
void ntt_combine_push(ntt_combine_t* self, ntt_queue_t* queue);

/**
 * @brief Take first queue from loop queue.
 *
 * @return The pointer to next queue or NULL if no more queues.
 */
ntt_queue_t* ntt_combine_try_take(ntt_combine_t* self);

/**
 * @brief Execute loop queue.
 */
void ntt_combine_svc(ntt_combine_t* self);

/**
 * @brief Execute loop queue.
 *
 * @pre loop_limit > 0
 * @pre queue_limit > 0
 *
 * @return Return 1 if need svc again, otherwise 0.
 */
int ntt_combine_svc_with_limits(ntt_combine_t* self, size_t loop_limit, size_t queue_limit);

EXTERN_STOP
