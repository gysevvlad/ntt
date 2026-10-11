#pragma once

#include "ntt/defs.h"

EXTERN_START

#include <stdint.h>

typedef struct ntt_slab ntt_slab_t;

/**
 * @brief Capacity the heap starts with.
 */
#ifndef NTT_QUEUE_HEAP_MIN_CAPACITY
#define NTT_QUEUE_HEAP_MIN_CAPACITY 8
#endif

/**
 * @brief Heap entry: a slot of the queue slab and the deadline it asked for.
 *
 * The generation is the one the queue had when the entry was pushed. The queue
 * bumps it whenever it registers again or drops its registration, so an entry
 * whose generation no longer matches is a tombstone.
 */
typedef struct ntt_queue_heap_elem {
    uint64_t deadline;
    uint32_t index;
    uint32_t generation;
} ntt_queue_heap_elem_t;

/**
 * @brief Binary min-heap of queue registrations, keyed by deadline.
 *
 * Entries are never removed on purpose: dropping a registration is a bump of
 * the generation of the queue, and the entry that is left behind is dropped
 * when it reaches the top, or in bulk once tombstones take over half the heap.
 * Nothing in the queue points back here, so sifting writes to this array only.
 *
 * The structure carries no lock. The timer service owns one and serializes
 * every call.
 */
typedef struct ntt_queue_heap {
    ntt_queue_heap_elem_t* elems;
    uint32_t size;

    /**
     * @brief How many entries are believed to be tombstones.
     *
     * A hint for when to rebuild, not an exact count: a queue that drops its
     * registration only bumps its generation and never reaches the service, so
     * the tombstone it leaves is not counted here.
     */
    uint32_t tombstones;

    uint32_t capacity;
} ntt_queue_heap_t;

void ntt_queue_heap_init(ntt_queue_heap_t* self);

void ntt_queue_heap_deinit(ntt_queue_heap_t* self);

/**
 * @brief Check whether any entry still stands.
 *
 * Walks the whole array, so this is for assertions only.
 *
 * @return Returns 1 when a live entry was found, otherwise 0.
 */
int ntt_queue_heap_has_live(
    const ntt_queue_heap_t* self,
    const ntt_slab_t* slab);

/**
 * @brief Read the earliest deadline in the heap, tombstones included.
 *
 * Deliberately does not look at liveness: that would mean dereferencing a
 * foreign queue inside the critical section of the service. Arming the timerfd
 * at the deadline of a tombstone only costs one spurious wakeup, which the
 * tick handler drops.
 *
 * @return Returns 1 if a deadline was written, otherwise 0.
 */
static inline int ntt_queue_heap_top(
    const ntt_queue_heap_t* self,
    uint64_t* deadline)
{
    if (self->size == 0) {
        return 0;
    }

    *deadline = self->elems[0].deadline;

    return 1;
}

/**
 * @brief Add a registration.
 */
void ntt_queue_heap_push(
    ntt_queue_heap_t* self,
    uint32_t index,
    uint32_t generation,
    uint64_t deadline);

/**
 * @brief Take the earliest live registration out if its deadline has passed.
 *
 * Drops the tombstones it meets on the way.
 *
 * @return Returns 1 when an entry was written, otherwise 0.
 */
int ntt_queue_heap_take_expired(
    ntt_queue_heap_t* self,
    const ntt_slab_t* slab,
    uint64_t now,
    ntt_queue_heap_elem_t* elem);

/**
 * @brief Count one more tombstone and rebuild the heap if there are too many.
 *
 * Called when a registration is replaced, so that the heap does not grow with
 * entries nobody will ever look at.
 */
void ntt_queue_heap_retire(
    ntt_queue_heap_t* self,
    const ntt_slab_t* slab,
    uint32_t cnt);

EXTERN_STOP
