#pragma once

#include "ntt/defs.h"
#include "ntt/impl/atomic.h"

EXTERN_START

#include <pthread.h>
#include <stdint.h>

typedef struct ntt_queue ntt_queue_t;

/**
 * @brief Sentinel for a queue that does not come from the slab.
 */
#define NTT_QUEUE_SLAB_NONE UINT32_MAX

/**
 * @brief Queues per block of the slab.
 */
#ifndef NTT_QUEUE_SLAB_BLOCK
#define NTT_QUEUE_SLAB_BLOCK 64
#endif

/**
 * @brief Blocks the slab can hold.
 *
 * The table is fixed so that it is never moved: the timer service resolves an
 * index into a queue without holding the slab lock, and a table that could be
 * reallocated would race with that.
 */
#ifndef NTT_QUEUE_SLAB_BLOCKS
#define NTT_QUEUE_SLAB_BLOCKS 1024
#endif

NTT_STATIC_ASSERT(
    NTT_QUEUE_SLAB_BLOCK > 0
        && (NTT_QUEUE_SLAB_BLOCK & (NTT_QUEUE_SLAB_BLOCK - 1)) == 0,
    "ntt exception: NTT_QUEUE_SLAB_BLOCK must be a power of two");

/**
 * @brief Slab of queue objects, with a generation per slot.
 *
 * Storage of a queue never moves and is never given back while the loop runs,
 * so an index into the slab is always safe to resolve. That is what lets the
 * timer service keep stale entries: it can look at the generation of a slot
 * whose queue is long gone, see that it does not match and drop the entry,
 * which is why cancelling a registration costs nothing.
 *
 * The generation of a slot survives its reuse: it is owned by the slab and is
 * not touched by ntt_queue_init.
 */
typedef struct ntt_queue_slab {
    pthread_spinlock_t lock;

    /**
     * @brief Block table.
     *
     * Published atomically, because a slot is resolved into a queue without
     * taking the lock of the slab: the timer service does it while holding its
     * own lock, which says nothing about this one.
     */
    atomic_uintptr_t blocks[NTT_QUEUE_SLAB_BLOCKS];

    /** Slots handed out so far, published atomically for the same reason. */
    atomic_uint_fast32_t size;

    /** Blocks allocated so far. Guarded by the lock. */
    uint32_t block_cnt;

    /** Head of the free slot list, NTT_QUEUE_SLAB_NONE when empty. */
    uint32_t free_head;
} ntt_queue_slab_t;

void ntt_queue_slab_init(ntt_queue_slab_t* self);

/**
 * @brief Release every block.
 *
 * @pre No queue is alive, which holds once every worker has joined.
 */
void ntt_queue_slab_deinit(ntt_queue_slab_t* self);

/**
 * @brief Take a slot and return the storage of its queue.
 *
 * The queue is not initialized: the caller runs ntt_queue_init on it.
 *
 * @param[out] index The slot of the queue.
 */
ntt_queue_t* ntt_queue_slab_take(ntt_queue_slab_t* self, uint32_t* index);

/**
 * @brief Give a slot back and bump its generation.
 *
 * Bumping the generation turns every entry that still refers to this queue
 * into a tombstone, including the entries of a later queue would-be reuse.
 */
void ntt_queue_slab_give(ntt_queue_slab_t* self, uint32_t index);

/**
 * @brief The queue of a slot.
 *
 * Safe for any index the slab has ever handed out, even when the queue it
 * belonged to is gone: the storage stays valid for the life of the loop.
 */
ntt_queue_t* ntt_queue_slab_at(const ntt_queue_slab_t* self, uint32_t index);

EXTERN_STOP
