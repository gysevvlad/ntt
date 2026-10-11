#include "./queue_heap.h"

#include "./malloc.h"
#include "./queue.h"
#include "./slab.h"

#include <assert.h>

static inline int ntt_queue_heap_less(
    const ntt_queue_heap_elem_t* lhs,
    const ntt_queue_heap_elem_t* rhs)
{
    return lhs->deadline < rhs->deadline ? 1 : 0;
}

/**
 * @brief Check that an entry still refers to the registration that pushed it.
 */
static inline int ntt_queue_heap_live(
    const ntt_slab_t* slab,
    const ntt_queue_heap_elem_t* elem)
{
    ntt_queue_t* queue = ntt_slab_at(slab, elem->index);

    return ntt_queue_timer_generation(queue) == elem->generation ? 1 : 0;
}

static void ntt_queue_heap_sift_up(
    ntt_queue_heap_t* self,
    uint32_t pos,
    ntt_queue_heap_elem_t elem)
{
    while (pos > 0) {
        uint32_t parent = (pos - 1) / 2;

        if (!ntt_queue_heap_less(&elem, &self->elems[parent])) {
            break;
        }

        self->elems[pos] = self->elems[parent];
        pos              = parent;
    }

    self->elems[pos] = elem;
}

static void ntt_queue_heap_sift_down(
    ntt_queue_heap_t* self,
    uint32_t pos,
    ntt_queue_heap_elem_t elem)
{
    for (;;) {
        uint32_t left = (pos * 2) + 1;

        if (left >= self->size) {
            break;
        }

        uint32_t child = left;
        uint32_t right = left + 1;

        if (right < self->size
            && ntt_queue_heap_less(&self->elems[right], &self->elems[left])) {
            child = right;
        }

        if (!ntt_queue_heap_less(&self->elems[child], &elem)) {
            break;
        }

        self->elems[pos] = self->elems[child];
        pos              = child;
    }

    self->elems[pos] = elem;
}

static void ntt_queue_heap_reserve(ntt_queue_heap_t* self, uint32_t cnt)
{
    if (cnt <= self->capacity) {
        return;
    }

    uint32_t capacity = self->capacity == 0
        ? NTT_QUEUE_HEAP_MIN_CAPACITY
        : self->capacity;

    while (capacity < cnt) {
        capacity *= 2;
    }

    self->elems = ntt_realloc_or_die(
        self->elems,
        sizeof(ntt_queue_heap_elem_t) * capacity);
    self->capacity = capacity;
}

static void ntt_queue_heap_pop_root(ntt_queue_heap_t* self)
{
    assert(self->size > 0);

    uint32_t last = self->size - 1;

    if (last == 0) {
        self->size = 0;
        return;
    }

    ntt_queue_heap_elem_t elem = self->elems[last];

    self->size = last;
    ntt_queue_heap_sift_down(self, 0, elem);
}

/**
 * @brief Drop the tombstones that sit on top of the heap.
 */
static void ntt_queue_heap_drop_dead_root(
    ntt_queue_heap_t* self,
    const ntt_slab_t* slab)
{
    while (self->size > 0 && !ntt_queue_heap_live(slab, &self->elems[0])) {
        ntt_queue_heap_pop_root(self);

        // Saturating: the counter is a hint and may not have seen this one.
        if (self->tombstones > 0) {
            self->tombstones -= 1;
        }
    }
}

/**
 * @brief Reclaim tombstones in bulk once they take over half of the heap.
 */
static void ntt_queue_heap_compact(
    ntt_queue_heap_t* self,
    const ntt_slab_t* slab)
{
    if (self->tombstones * 2 <= self->size) {
        return;
    }

    uint32_t out = 0;
    uint32_t pos = 0;
    for (; pos < self->size; ++pos) {
        if (ntt_queue_heap_live(slab, &self->elems[pos])) {
            self->elems[out] = self->elems[pos];
            out += 1;
        }
    }

    self->size       = out;
    self->tombstones = 0;

    if (self->size > 1) {
        uint32_t i = self->size / 2;
        while (i-- > 0) {
            ntt_queue_heap_sift_down(self, i, self->elems[i]);
        }
    }
}

void ntt_queue_heap_init(ntt_queue_heap_t* self)
{
    self->elems      = NULL;
    self->size       = 0;
    self->tombstones = 0;
    self->capacity   = 0;
}

int ntt_queue_heap_has_live(
    const ntt_queue_heap_t* self,
    const ntt_slab_t* slab)
{
    uint32_t pos = 0;
    for (; pos < self->size; ++pos) {
        if (ntt_queue_heap_live(slab, &self->elems[pos])) {
            return 1;
        }
    }

    return 0;
}

void ntt_queue_heap_deinit(ntt_queue_heap_t* self)
{
    ntt_free(self->elems);
    ntt_queue_heap_init(self);
}

void ntt_queue_heap_push(
    ntt_queue_heap_t* self,
    uint32_t index,
    uint32_t generation,
    uint64_t deadline)
{
    ntt_queue_heap_reserve(self, self->size + 1);

    uint32_t pos = self->size;
    self->size   = pos + 1;

    ntt_queue_heap_elem_t elem;
    elem.deadline   = deadline;
    elem.index      = index;
    elem.generation = generation;

    ntt_queue_heap_sift_up(self, pos, elem);
}

int ntt_queue_heap_take_expired(
    ntt_queue_heap_t* self,
    const ntt_slab_t* slab,
    uint64_t now,
    ntt_queue_heap_elem_t* elem)
{
    ntt_queue_heap_drop_dead_root(self, slab);

    if (self->size == 0 || self->elems[0].deadline > now) {
        return 0;
    }

    *elem = self->elems[0];

    ntt_queue_heap_pop_root(self);

    return 1;
}

void ntt_queue_heap_retire(
    ntt_queue_heap_t* self,
    const ntt_slab_t* slab,
    uint32_t cnt)
{
    self->tombstones += cnt;

    if (self->tombstones > self->size) {
        // The count is a hint: entries may already have been dropped at the
        // top, so it can run ahead of what the array actually holds.
        self->tombstones = self->size;
    }

    ntt_queue_heap_compact(self, slab);
}
