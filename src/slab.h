#pragma once

#include "ntt/defs.h"
#include "ntt/impl/atomic.h"

EXTERN_START

#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Objects in the first block, as a power of two.
 *
 * Each block after the first is twice as large as the previous one. The size
 * is given as an exponent because block lengths must be powers of two, and the 
 * exponent also determines the size of the block table.
 */
#ifndef NTT_FIRST_SLAB_BLOCK_LEN_LOG2
#define NTT_FIRST_SLAB_BLOCK_LEN_LOG2 6
#endif

/**
 * @brief Bits of a slab index, which is carried in a uint32_t.
 */
#define NTT_SLAB_INDEX_BITS (sizeof(uint32_t) * CHAR_BIT)

NTT_STATIC_ASSERT(
    NTT_FIRST_SLAB_BLOCK_LEN_LOG2 < NTT_SLAB_INDEX_BITS,
    "ntt exception: the first slab block does not fit an index");

#define NTT_FIRST_SLAB_BLOCK_LEN (1u << NTT_FIRST_SLAB_BLOCK_LEN_LOG2)

/**
 * @brief Objects an index can address.
 *
 * One value of an index is kept aside to mean no object, so the rest address
 * one each. The count therefore still fits an index itself.
 */
#define NTT_SLAB_CAPACITY \
    ((uint32_t)((((uint64_t)1 << NTT_SLAB_INDEX_BITS)) - 1))

/**
 * @brief The index that means no object.
 *
 * The first value past the capacity, so it can never be an object.
 */
#define NTT_SLAB_NONE NTT_SLAB_CAPACITY

/**
 * @brief Blocks whose size follows the doubling sequence.
 *
 * They hold F * (2^n - 1) objects, which is F short of the whole index.
 */
#define NTT_SLAB_FULL_BLOCKS (NTT_SLAB_INDEX_BITS - NTT_FIRST_SLAB_BLOCK_LEN_LOG2)

/**
 * @brief Objects of the block that covers what the sequence leaves over.
 *
 * Zero when the first block holds a single object, because then the sequence
 * already ends at the index and no such block exists.
 */
#define NTT_SLAB_LAST_BLOCK_LEN (NTT_FIRST_SLAB_BLOCK_LEN - 1)

/**
 * @brief Entries of the block table.
 *
 * The table is fixed so that it is never moved: an index is resolved into an
 * object without taking the lock.
 */
#define NTT_SLAB_BLOCKS \
    (NTT_SLAB_FULL_BLOCKS + (NTT_SLAB_LAST_BLOCK_LEN != 0 ? 1 : 0))

/**
 * @brief First member of every object kept in a slab.
 *
 * The generation is owned by the slot and survives its reuse, which is what
 * lets a stale reference to the object be recognised: whoever holds an index
 * and the generation it saw can tell that the object is gone, even when the
 * slot already belongs to somebody else.
 */
typedef struct ntt_slab_head {
    atomic_uint_fast32_t generation;

    /** Slot of this object. */
    uint32_t index;

    /** Free list link, meaningless while the object is alive. */
    uint32_t next_free;
} ntt_slab_head_t;

/**
 * @brief Slab of equally sized objects.
 *
 * Storage never moves and is never given back while the slab lives, so an
 * index into it is always safe to resolve. Objects are padded to a cache line
 * apart, so that two of them in neighbouring slots do not share one.
 */
typedef struct ntt_slab {
    pthread_spinlock_t lock;

    /** Published atomically: readers resolve an index without the lock. */
    atomic_uintptr_t blocks[NTT_SLAB_BLOCKS];

    /** Slots handed out so far, published atomically for the same reason. */
    atomic_uint_fast32_t size;

    /** Bytes between two objects of a block. */
    uint32_t stride;

    /** Blocks allocated so far. Guarded by the lock. */
    uint32_t block_cnt;

    /** Head of the free list, NTT_SLAB_NONE when empty. */
    uint32_t free_head;
} ntt_slab_t;

/**
 * @brief Initialize a slab of objects of the given size.
 *
 * @pre The object embeds ntt_slab_head_t as its first member.
 */
void ntt_slab_init(ntt_slab_t* self, size_t object_size);

/**
 * @pre No object is alive.
 */
void ntt_slab_deinit(ntt_slab_t* self);

/**
 * @brief Take a slot and return the storage of its object.
 *
 * The object is not initialized beyond its head, which the slab owns.
 */
void* ntt_slab_take(ntt_slab_t* self);

/**
 * @brief Give a slot back and bump its generation.
 */
void ntt_slab_give(ntt_slab_t* self, uint32_t index);

/**
 * @brief The block an index falls into.
 *
 * Block k holds NTT_FIRST_SLAB_BLOCK_LEN << k objects, so the blocks start at
 * NTT_FIRST_SLAB_BLOCK_LEN * ((1 << k) - 1) and the block of an index is the
 * position of the highest bit of (index >> the exponent) + 1.
 */
static inline uint32_t ntt_slab_block_of(uint32_t index)
{
    return 31u - (uint32_t)__builtin_clz((index >> NTT_FIRST_SLAB_BLOCK_LEN_LOG2) + 1u);
}

static inline uint32_t ntt_slab_block_first(uint32_t block)
{
    return NTT_FIRST_SLAB_BLOCK_LEN * ((1u << block) - 1u);
}

static inline uint32_t ntt_slab_block_len(uint32_t block)
{
    return (uint32_t)NTT_FIRST_SLAB_BLOCK_LEN << block;
}

/**
 * @brief The object of a slot.
 *
 * Safe for any index the slab has ever handed out, even when the object it
 * belonged to is gone.
 */
static inline void* ntt_slab_at(const ntt_slab_t* self, uint32_t index)
{
    assert(index < atomic_load_explicit(&self->size, memory_order_acquire));

    uint32_t block = ntt_slab_block_of(index);

    uintptr_t base = atomic_load_explicit(
        &self->blocks[block],
        memory_order_acquire);

    return (char*)base + ((index - ntt_slab_block_first(block)) * self->stride);
}

static inline uint32_t ntt_slab_index(const void* object)
{
    return ((const ntt_slab_head_t*)object)->index;
}

/**
 * @brief The generation of the object, as it stands now.
 */
static inline uint32_t ntt_slab_generation(const void* object)
{
    return (uint32_t)atomic_load_explicit(
        &((const ntt_slab_head_t*)object)->generation,
        memory_order_acquire);
}

/**
 * @brief Bump the generation, retiring every reference that saw the old one.
 */
static inline uint32_t ntt_slab_retire(void* object)
{
    return (uint32_t)atomic_fetch_add_explicit(
               &((ntt_slab_head_t*)object)->generation,
               1,
               memory_order_release)
        + 1;
}

EXTERN_STOP
