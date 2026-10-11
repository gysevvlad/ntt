#include "./slab.h"

#include "./die.h"
#include "./malloc.h"

#include <assert.h>

/**
 * @brief Like ntt_slab_at, but for a slot that is not published yet.
 */
static void* ntt_slab_fresh_at(const ntt_slab_t* self, uint32_t index)
{
    uint32_t block = ntt_slab_block_of(index);

    uintptr_t base = atomic_load_explicit(
        &self->blocks[block],
        memory_order_relaxed);

    return (char*)base + ((index - ntt_slab_block_first(block)) * self->stride);
}

void ntt_slab_init(ntt_slab_t* self, size_t object_size)
{
    pthread_spin_init(&self->lock, 0);

    unsigned i = 0;
    for (; i < NTT_SLAB_BLOCKS; ++i) {
        atomic_store_explicit(&self->blocks[i], 0, memory_order_relaxed);
    }

    self->stride    = (uint32_t)(((object_size + 63) / 64) * 64);
    self->block_cnt = 0;
    self->free_head = NTT_SLAB_NONE;
    atomic_store_explicit(&self->size, 0, memory_order_relaxed);
}

void ntt_slab_deinit(ntt_slab_t* self)
{
    uint32_t i = 0;
    for (; i < self->block_cnt; ++i) {
        ntt_free((void*)atomic_load_explicit(&self->blocks[i], memory_order_relaxed));
        atomic_store_explicit(&self->blocks[i], 0, memory_order_relaxed);
    }

    self->block_cnt = 0;
    self->free_head = NTT_SLAB_NONE;
    atomic_store_explicit(&self->size, 0, memory_order_relaxed);

    pthread_spin_destroy(&self->lock);
}

void* ntt_slab_take(ntt_slab_t* self)
{
    pthread_spin_lock(&self->lock);

    uint32_t slot = self->free_head;

    if (slot != NTT_SLAB_NONE) {
        ntt_slab_head_t* head = ntt_slab_fresh_at(self, slot);
        self->free_head       = head->next_free;
    } else {
        slot = (uint32_t)atomic_load_explicit(&self->size, memory_order_relaxed);

        if (slot == NTT_SLAB_CAPACITY) {
            // Out of indices. Unreachable in practice: that many objects of
            // one cache line each is hundreds of gigabytes.
            ntt_die();
        }

        if (slot == ntt_slab_block_first(self->block_cnt)) {
            uint32_t count = self->block_cnt == NTT_SLAB_FULL_BLOCKS
                ? NTT_SLAB_LAST_BLOCK_LEN
                : ntt_slab_block_len(self->block_cnt);

            atomic_store_explicit(
                &self->blocks[self->block_cnt],
                (uintptr_t)ntt_aligned_or_die(64, (size_t)self->stride * count),
                memory_order_release);

            self->block_cnt += 1;
        }

        ntt_slab_head_t* head = ntt_slab_fresh_at(self, slot);
        head->index           = slot;
        atomic_store_explicit(&head->generation, 1, memory_order_relaxed);

        // Published last: a reader that sees this index also sees its block.
        atomic_store_explicit(&self->size, slot + 1, memory_order_release);
    }

    pthread_spin_unlock(&self->lock);

    return ntt_slab_fresh_at(self, slot);
}

void ntt_slab_give(ntt_slab_t* self, uint32_t index)
{
    ntt_slab_head_t* head = ntt_slab_at(self, index);

    // Every reference that still carries the old generation is now stale,
    // including the ones a later tenant of this slot would see.
    ntt_slab_retire(head);

    pthread_spin_lock(&self->lock);

    head->next_free = self->free_head;
    self->free_head = index;

    pthread_spin_unlock(&self->lock);
}
