#pragma once

#include "./queue.h"

EXTERN_START

#include <pthread.h>

/**
 * @brief Queue of ntt queues.
 */
typedef struct ntt_combine {
    pthread_spinlock_t lock;
    ntt_list_t list;
} ntt_combine_t;

/**
 * @brief Init ntt loop queue.
 */
void ntt_combine_init(ntt_combine_t* self);

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
 * @brief Push ntt_queue_t to the combine.
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
 *
 * @pre loop_limit > 0
 * @pre queue_limit > 0
 *
 * @return Return 1 if need svc again, otherwise 0.
 */
int ntt_combine_svc(ntt_combine_t* self, size_t loop_limit, size_t queue_limit);

EXTERN_STOP
