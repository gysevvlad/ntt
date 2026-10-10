#pragma once

#include "ntt/defs.h"

EXTERN_START

#include <stdint.h>

typedef struct ntt_queue ntt_queue_t;

/**
 * @brief Sentinel for a queue that is not in the heap.
 */
#define NTT_QUEUE_HEAP_NONE UINT32_MAX

/**
 * @brief Capacity the heap starts with.
 */
#ifndef NTT_QUEUE_HEAP_MIN_CAPACITY
#define NTT_QUEUE_HEAP_MIN_CAPACITY 8
#endif

/**
 * @brief Heap entry: a queue and the earliest deadline it asked to be woken at.
 */
typedef struct ntt_queue_heap_elem {
    uint64_t deadline;
    ntt_queue_t* queue;
} ntt_queue_heap_elem_t;

/**
 * @brief Binary min-heap of queues, keyed by their earliest deadline.
 *
 * Holds at most one entry per queue. A queue remembers its position in the
 * heap, so moving its key and removing it are O(log n) without a search.
 *
 * Unlike the timer heap of a queue, removal here is eager: entries are few,
 * one per queue with armed timers, and the invariant of one entry per queue
 * is worth more than a cheaper cancel.
 *
 * The structure carries no lock. The timer service owns one and serializes
 * every call.
 */
typedef struct ntt_queue_heap {
    ntt_queue_heap_elem_t* elems;
    uint32_t size;
    uint32_t capacity;
} ntt_queue_heap_t;

void ntt_queue_heap_init(ntt_queue_heap_t* self);

void ntt_queue_heap_deinit(ntt_queue_heap_t* self);

static inline int ntt_queue_heap_empty(const ntt_queue_heap_t* self)
{
    return self->size == 0 ? 1 : 0;
}

/**
 * @brief Read the earliest deadline in the heap.
 *
 * @return Returns 1 if a deadline was written, otherwise 0.
 */
static inline int ntt_queue_heap_top(const ntt_queue_heap_t* self, uint64_t* deadline)
{
    if (self->size == 0) {
        return 0;
    }
    *deadline = self->elems[0].deadline;
    return 1;
}

/**
 * @brief Add a queue to the heap.
 *
 * @pre The queue is not in the heap.
 */
void ntt_queue_heap_insert(
    ntt_queue_heap_t* self,
    ntt_queue_t* queue,
    uint64_t deadline);

/**
 * @brief Move the deadline of a queue that is already in the heap earlier.
 *
 * @pre The queue is in the heap and the deadline is not later than its
 *      current one.
 */
void ntt_queue_heap_decrease(
    ntt_queue_heap_t* self,
    ntt_queue_t* queue,
    uint64_t deadline);

/**
 * @brief Take a queue out of the heap.
 *
 * @pre The queue is in the heap.
 */
void ntt_queue_heap_remove(ntt_queue_heap_t* self, ntt_queue_t* queue);

/**
 * @brief Take the earliest queue out of the heap if its deadline has passed.
 *
 * @return The queue, or NULL when nothing has expired.
 */
ntt_queue_t* ntt_queue_heap_take_expired(ntt_queue_heap_t* self, uint64_t now);

EXTERN_STOP
