#include "./timer_heap.h"

#include "./malloc.h"
#include "./task.h"

#include <assert.h>
#include <stdint.h>

#if UINTPTR_MAX == 0xffffffffffffffffULL
NTT_STATIC_ASSERT(sizeof(ntt_timer_heap_elem_t) == 24, "unexpected heap entry layout");
NTT_STATIC_ASSERT(sizeof(ntt_timer_slot_t) == 16, "unexpected slab entry layout");
#endif

static inline ntt_timer_heap_elem_t* ntt_timer_heap_elem_at(
    const ntt_timer_heap_t* self,
    uint32_t pos)
{
    assert(pos < self->size);
    return &self->heap[pos];
}

static inline ntt_timer_slot_t* ntt_timer_heap_slot_at(
    const ntt_timer_heap_t* self,
    uint32_t index)
{
    assert(index < self->slot_cnt);
    return &self->slots[index];
}

/**
 * @brief Compare two heap entries.
 *
 * Orders by deadline and breaks ties by sequence number, which keeps the heap
 * stable. Both live in the entry, so sifting never reads the slab.
 *
 * @return Returns 1 if lhs must fire before rhs, otherwise 0.
 */
static inline int ntt_timer_heap_less(
    const ntt_timer_heap_elem_t* lhs,
    const ntt_timer_heap_elem_t* rhs)
{
    if (lhs->deadline != rhs->deadline) {
        return lhs->deadline < rhs->deadline ? 1 : 0;
    }
    return lhs->seq < rhs->seq ? 1 : 0;
}

/**
 * @brief Check that an entry still refers to the timer that pushed it.
 *
 * @return Returns 1 for a live entry and 0 for a tombstone.
 */
static inline int ntt_timer_heap_elem_live(
    const ntt_timer_heap_t* self,
    const ntt_timer_heap_elem_t* elem)
{
    return ntt_timer_heap_slot_at(self, elem->index)->version == elem->version ? 1 : 0;
}

/**
 * @brief Write an entry at the given position.
 */
static inline void ntt_timer_heap_place(
    ntt_timer_heap_t* self,
    uint32_t pos,
    ntt_timer_heap_elem_t elem)
{
    *ntt_timer_heap_elem_at(self, pos) = elem;
}

static void ntt_timer_heap_sift_up(
    ntt_timer_heap_t* self,
    uint32_t pos,
    ntt_timer_heap_elem_t elem)
{
    while (pos > 0) {
        uint32_t parent                = (pos - 1) / 2;
        ntt_timer_heap_elem_t parent_elem = *ntt_timer_heap_elem_at(self, parent);

        if (!ntt_timer_heap_less(&elem, &parent_elem)) {
            break;
        }

        ntt_timer_heap_place(self, pos, parent_elem);
        pos = parent;
    }

    ntt_timer_heap_place(self, pos, elem);
}

static void ntt_timer_heap_sift_down(
    ntt_timer_heap_t* self,
    uint32_t pos,
    ntt_timer_heap_elem_t elem)
{
    for (;;) {
        uint32_t left = (pos * 2) + 1;

        if (left >= self->size) {
            break;
        }

        uint32_t right = left + 1;
        uint32_t child = left;

        if (right < self->size
            && ntt_timer_heap_less(
                ntt_timer_heap_elem_at(self, right),
                ntt_timer_heap_elem_at(self, left))) {
            child = right;
        }

        ntt_timer_heap_elem_t child_elem = *ntt_timer_heap_elem_at(self, child);

        if (!ntt_timer_heap_less(&child_elem, &elem)) {
            break;
        }

        ntt_timer_heap_place(self, pos, child_elem);
        pos = child;
    }

    ntt_timer_heap_place(self, pos, elem);
}

/**
 * @brief Pick the next capacity that can hold cnt entries.
 */
static uint32_t ntt_timer_heap_grow_capacity(uint32_t cap, uint32_t cnt)
{
    uint32_t new_cap = cap == 0 ? NTT_TIMER_MIN_CAPACITY : cap;

    while (new_cap < cnt) {
        new_cap *= 2;
    }

    return new_cap;
}

/**
 * @brief Make sure the heap can hold an entry at the given position.
 */
static void ntt_timer_heap_reserve_elem(ntt_timer_heap_t* self, uint32_t pos)
{
    if (pos < self->heap_cap) {
        return;
    }

    uint32_t new_cap = ntt_timer_heap_grow_capacity(self->heap_cap, pos + 1);

    self->heap = ntt_realloc_or_die(
        self->heap,
        sizeof(ntt_timer_heap_elem_t) * new_cap);
    self->heap_cap = new_cap;
}

/**
 * @brief Make sure the slab can hold the given slot index.
 */
static void ntt_timer_heap_reserve_slot(ntt_timer_heap_t* self, uint32_t index)
{
    if (index < self->slot_cap) {
        return;
    }

    uint32_t new_cap = ntt_timer_heap_grow_capacity(self->slot_cap, index + 1);

    self->slots = ntt_realloc_or_die(
        self->slots,
        sizeof(ntt_timer_slot_t) * new_cap);
    self->slot_cap = new_cap;
}

/**
 * @brief Give back heap capacity that is no longer needed.
 *
 * Halves the array only once it is three quarters empty, so that pushes and
 * pops around the boundary do not reallocate on every call. The slab is never
 * shrunk: slot indices are handles, so its entries cannot be moved.
 */
static void ntt_timer_heap_shrink(ntt_timer_heap_t* self)
{
    if (self->heap_cap <= NTT_TIMER_MIN_CAPACITY) {
        return;
    }

    if (self->size * 4 > self->heap_cap) {
        return;
    }

    uint32_t new_cap = self->heap_cap / 2;

    self->heap = ntt_realloc_or_die(
        self->heap,
        sizeof(ntt_timer_heap_elem_t) * new_cap);
    self->heap_cap = new_cap;
}

static uint32_t ntt_timer_heap_alloc_slot(ntt_timer_heap_t* self)
{
    if (self->free_head != NTT_TIMER_SLOT_NONE) {
        uint32_t index = self->free_head;

        ntt_timer_slot_t* slot = ntt_timer_heap_slot_at(self, index);
        self->free_head        = slot->next_free;

        // vacant (even) -> occupied (odd)
        slot->version += 1;
        assert((slot->version & 1u) == 1u);

        return index;
    }

    uint32_t index = self->slot_cnt;

    ntt_timer_heap_reserve_slot(self, index);
    self->slot_cnt += 1;

    ntt_timer_heap_slot_at(self, index)->version = 1;

    return index;
}

static void ntt_timer_heap_free_slot(ntt_timer_heap_t* self, uint32_t index)
{
    ntt_timer_slot_t* slot = ntt_timer_heap_slot_at(self, index);

    slot->task = NULL;

    // occupied (odd) -> vacant (even)
    slot->version += 1;
    assert((slot->version & 1u) == 0u);

    slot->next_free = self->free_head;
    self->free_head = index;
}

/**
 * @brief Drop the root entry of the heap.
 */
static void ntt_timer_heap_pop_root(ntt_timer_heap_t* self)
{
    assert(self->size > 0);

    uint32_t last = self->size - 1;

    if (last == 0) {
        self->size = 0;
        ntt_timer_heap_shrink(self);
        return;
    }

    ntt_timer_heap_elem_t elem = *ntt_timer_heap_elem_at(self, last);

    self->size = last;
    ntt_timer_heap_sift_down(self, 0, elem);

    ntt_timer_heap_shrink(self);
}

/**
 * @brief Drop the tombstones that sit on top of the heap.
 */
static void ntt_timer_heap_drop_dead_root(ntt_timer_heap_t* self)
{
    while (self->size > 0
        && !ntt_timer_heap_elem_live(self, ntt_timer_heap_elem_at(self, 0))) {
        ntt_timer_heap_pop_root(self);
        assert(self->tombstones > 0);
        self->tombstones -= 1;
    }
}

/**
 * @brief Reclaim tombstones in bulk once they take over half of the heap.
 *
 * Filters them out in place and rebuilds the heap with Floyd in O(n), which
 * is cheaper per tombstone than popping them one at a time at O(log n).
 */
static void ntt_timer_heap_compact(ntt_timer_heap_t* self)
{
    if (self->tombstones * 2 <= self->size) {
        return;
    }

    uint32_t out = 0;
    uint32_t pos = 0;
    for (; pos < self->size; ++pos) {
        ntt_timer_heap_elem_t elem = *ntt_timer_heap_elem_at(self, pos);
        if (ntt_timer_heap_elem_live(self, &elem)) {
            // size is only shrunk afterwards, so writing below pos is safe
            *ntt_timer_heap_elem_at(self, out) = elem;
            out += 1;
        }
    }

    self->size       = out;
    self->tombstones = 0;

    if (self->size > 1) {
        uint32_t i = self->size / 2;
        while (i-- > 0) {
            ntt_timer_heap_sift_down(self, i, *ntt_timer_heap_elem_at(self, i));
        }
    }

    ntt_timer_heap_shrink(self);
}

void ntt_timer_heap_init(ntt_timer_heap_t* self)
{
    self->heap     = NULL;
    self->heap_cap = 0;

    self->slots    = NULL;
    self->slot_cap = 0;

    self->size       = 0;
    self->tombstones = 0;
    self->slot_cnt   = 0;
    self->free_head = NTT_TIMER_SLOT_NONE;
    self->seq_next  = 0;
}

void ntt_timer_heap_deinit(ntt_timer_heap_t* self)
{
    uint32_t pos = 0;
    for (; pos < self->size; ++pos) {
        ntt_timer_heap_elem_t* elem = ntt_timer_heap_elem_at(self, pos);
        if (ntt_timer_heap_elem_live(self, elem)) {
            ntt_task_destroy_inl(ntt_timer_heap_slot_at(self, elem->index)->task);
        }
    }
    self->size       = 0;
    self->tombstones = 0;

    ntt_free(self->heap);
    ntt_free(self->slots);

    ntt_timer_heap_init(self);
}

int ntt_timer_heap_peek(ntt_timer_heap_t* self, uint64_t* deadline)
{
    ntt_timer_heap_drop_dead_root(self);

    if (self->size == 0) {
        return 0;
    }

    *deadline = ntt_timer_heap_elem_at(self, 0)->deadline;

    return 1;
}

ntt_timer_tag_t ntt_timer_heap_push(
    ntt_timer_heap_t* self,
    uint64_t deadline,
    ntt_task_t* task)
{
    assert(task != NULL);
    assert(ntt_task_unlinked(task) == 1);

    uint32_t index = ntt_timer_heap_alloc_slot(self);

    ntt_timer_slot_t* slot = ntt_timer_heap_slot_at(self, index);
    slot->task             = task;

    uint32_t pos = self->size;
    ntt_timer_heap_reserve_elem(self, pos);
    self->size = pos + 1;

    ntt_timer_heap_elem_t elem;
    elem.deadline = deadline;
    elem.seq      = self->seq_next;
    elem.index    = index;
    elem.version  = slot->version;
    self->seq_next += 1;

    ntt_timer_heap_sift_up(self, pos, elem);

    ntt_timer_tag_t tag;
    tag.index   = index;
    tag.version = elem.version;

    return tag;
}

void ntt_timer_heap_cancel(ntt_timer_heap_t* self, ntt_timer_tag_t tag)
{
    assert(ntt_timer_tag_is_null(tag) == 0 && "[ntt_timer_heap] cancel of a null tag");
    assert(tag.index < self->slot_cnt && "[ntt_timer_heap] cancel of an unknown tag");

    if (ntt_unlikely(tag.index >= self->slot_cnt)) {
        return;
    }

    ntt_timer_slot_t* slot = ntt_timer_heap_slot_at(self, tag.index);

    assert(
        slot->version == tag.version
        && "[ntt_timer_heap] cancel of a timer that already fired or was cancelled");

    if (ntt_unlikely(slot->version != tag.version)) {
        return;
    }

    ntt_task_t* task = slot->task;

    // The entry is left in the heap as a tombstone and reclaimed later.
    ntt_timer_heap_free_slot(self, tag.index);
    self->tombstones += 1;

    ntt_task_destroy_inl(task);

    ntt_timer_heap_compact(self);
}

size_t ntt_timer_heap_svc(ntt_timer_heap_t* self, uint64_t now)
{
    size_t cnt = 0;

    for (;;) {
        if (self->size == 0) {
            break;
        }

        ntt_timer_heap_elem_t elem = *ntt_timer_heap_elem_at(self, 0);

        if (elem.deadline > now) {
            break;
        }

        ntt_timer_heap_pop_root(self);

        if (!ntt_timer_heap_elem_live(self, &elem)) {
            assert(self->tombstones > 0);
            self->tombstones -= 1;
            continue;
        }

        // The timer is retired before its task runs, so the task is free to
        // arm new timers and to cancel other armed ones.
        ntt_task_t* task = ntt_timer_heap_slot_at(self, elem.index)->task;
        ntt_timer_heap_free_slot(self, elem.index);

        ntt_task_svc_inl(task);
        ntt_task_destroy_inl(task);

        cnt += 1;
    }

    return cnt;
}
