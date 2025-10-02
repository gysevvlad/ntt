#pragma once

#include "ntt/defs.h"
#include "ntt/impl/atomic.h"

#include <assert.h>
#include <stdint.h>

struct ntt_spin {
    atomic_uint_fast64_t lock;
};

typedef struct ntt_spin ntt_spin_t;

#define NTT_SPIN_LOCK_MASK 0x8000000000000000

inline int ntt_spin_try_lock(ntt_spin_t* self, uint64_t* val)
{
    assert(self != NULL);
    assert(val != NULL);
    assert((*val & NTT_SPIN_LOCK_MASK) == 0);

    uint_fast64_t lock = atomic_load_explicit(&self->lock, memory_order_relaxed);

    for (;;) {
        if (ntt_unlikely(lock & NTT_SPIN_LOCK_MASK)) {
            uint_fast64_t new_lock = lock | *val;
            if (ntt_unlikely(!atomic_compare_exchange_weak(&self->lock, &lock, new_lock))) {
                continue;
            }
            return 0;
        }

        uint_fast64_t new_lock = lock | NTT_SPIN_LOCK_MASK | *val;
        if (ntt_unlikely(!atomic_compare_exchange_weak(&self->lock, &lock, new_lock))) {
            continue;
        }

        *val = (new_lock & ~NTT_SPIN_LOCK_MASK);
        return 1;
    }
}

inline int ntt_spin_try_unlock(ntt_spin_t* self, uint64_t* val)
{
    assert(self != NULL);
    assert(val != NULL);
    assert((*val & NTT_SPIN_LOCK_MASK) == 0);

    uint_fast64_t lock = NTT_SPIN_LOCK_MASK | *val;

    return 0;
}
