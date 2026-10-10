#include "./queue_slab.h"

#include "./die.h"
#include "./malloc.h"
#include "./queue.h"

#include <assert.h>
#include <stdlib.h>

/**
 * @brief Bytes between two queues of a block.
 *
 * Rounded up to a cache line so that the hot head of a queue, its reference
 * count, its task list, its lock and its combine node, never shares a line
 * with the queue next to it.
 */
#define NTT_QUEUE_SLAB_STRIDE \
    (((sizeof(ntt_queue_t) + 63) / 64) * 64)

static ntt_queue_t* ntt_queue_slab_block_at(
    const ntt_queue_slab_t* self,
    uint32_t index)
{
    uintptr_t block = atomic_load_explicit(
        &self->blocks[index / NTT_QUEUE_SLAB_BLOCK],
        memory_order_acquire);

    return (ntt_queue_t*)((char*)block
        + ((index % NTT_QUEUE_SLAB_BLOCK) * NTT_QUEUE_SLAB_STRIDE));
}

ntt_queue_t* ntt_queue_slab_at(const ntt_queue_slab_t* self, uint32_t index)
{
    assert(index < atomic_load_explicit(&self->size, memory_order_acquire));

    return ntt_queue_slab_block_at(self, index);
}

void ntt_queue_slab_init(ntt_queue_slab_t* self)
{
    pthread_spin_init(&self->lock, 0);

    unsigned i = 0;
    for (; i < NTT_QUEUE_SLAB_BLOCKS; ++i) {
        atomic_store_explicit(&self->blocks[i], 0, memory_order_relaxed);
    }

    self->block_cnt = 0;
    atomic_store_explicit(&self->size, 0, memory_order_relaxed);
    self->free_head = NTT_QUEUE_SLAB_NONE;
}

void ntt_queue_slab_deinit(ntt_queue_slab_t* self)
{
    uint32_t i = 0;
    for (; i < self->block_cnt; ++i) {
        ntt_free((void*)atomic_load_explicit(&self->blocks[i], memory_order_relaxed));
        atomic_store_explicit(&self->blocks[i], 0, memory_order_relaxed);
    }

    self->block_cnt = 0;
    atomic_store_explicit(&self->size, 0, memory_order_relaxed);
    self->free_head = NTT_QUEUE_SLAB_NONE;

    pthread_spin_destroy(&self->lock);
}

ntt_queue_t* ntt_queue_slab_take(ntt_queue_slab_t* self, uint32_t* index)
{
    pthread_spin_lock(&self->lock);

    uint32_t slot = self->free_head;

    if (slot != NTT_QUEUE_SLAB_NONE) {
        self->free_head = ntt_queue_slab_next_free(
            ntt_queue_slab_block_at(self, slot));
    } else {
        slot = (uint32_t)atomic_load_explicit(&self->size, memory_order_relaxed);

        if (slot / NTT_QUEUE_SLAB_BLOCK == self->block_cnt) {
            if (self->block_cnt == NTT_QUEUE_SLAB_BLOCKS) {
                // Raising NTT_QUEUE_SLAB_BLOCKS is the only way out: the table
                // is fixed so that it can be read without the lock.
                ntt_die();
            }

            atomic_store_explicit(
                &self->blocks[self->block_cnt],
                (uintptr_t)ntt_aligned_or_die(64, NTT_QUEUE_SLAB_STRIDE * NTT_QUEUE_SLAB_BLOCK),
                memory_order_release);
            self->block_cnt += 1;
        }

        // Published last: a reader that sees this index also sees its block.
        atomic_store_explicit(&self->size, slot + 1, memory_order_release);

        // A fresh slot starts its generations here. Reused slots keep the
        // generation they had, which is what makes stale entries detectable.
        ntt_queue_slab_init_generation(ntt_queue_slab_block_at(self, slot));
    }

    pthread_spin_unlock(&self->lock);

    *index = slot;

    return ntt_queue_slab_block_at(self, slot);
}

void ntt_queue_slab_give(ntt_queue_slab_t* self, uint32_t index)
{
    ntt_queue_t* queue = ntt_queue_slab_at(self, index);

    // Every entry of the timer service that still refers to this queue becomes
    // a tombstone, including the ones a later reuse of the slot would see.
    ntt_queue_slab_bump_generation(queue);

    pthread_spin_lock(&self->lock);

    ntt_queue_slab_set_next_free(queue, self->free_head);
    self->free_head = index;

    pthread_spin_unlock(&self->lock);
}
