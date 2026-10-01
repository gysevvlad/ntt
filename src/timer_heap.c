#include "./timer_heap.h"

#include "./malloc.h"
#include "./task.h"

#include <assert.h>
#include <stdint.h>

#if UINTPTR_MAX == 0xffffffffffffffffULL
NTT_STATIC_ASSERT(sizeof(ntt_timer_heap_elem_t) == 16, "unexpected heap entry layout");
NTT_STATIC_ASSERT(sizeof(ntt_timer_slot_t) == 24, "unexpected slab entry layout");
#endif

#define NTT_TIMER_BLOCK_TABLE_MIN 4

static inline ntt_timer_heap_elem_t* ntt_timer_heap_elem_at(
    const ntt_timer_heap_t* self,
    uint32_t pos)
{
    assert(pos < self->size);
    return &self->heap_blocks[pos / NTT_TIMER_BLOCK][pos % NTT_TIMER_BLOCK];
}

static inline ntt_timer_slot_t* ntt_timer_heap_slot_at(
    const ntt_timer_heap_t* self,
    uint32_t index)
{
    assert(index < self->slot_cnt);
    return &self->slot_blocks[index / NTT_TIMER_BLOCK][index % NTT_TIMER_BLOCK];
}

/**
 * @brief Compare two heap entries.
 *
 * Orders by deadline and breaks ties by sequence number, which keeps the
 * heap stable. The slab is touched only when the deadlines are equal.
 *
 * @return Returns 1 if lhs must fire before rhs, otherwise 0.
 */
static inline int ntt_timer_heap_less(
    const ntt_timer_heap_t* self,
    const ntt_timer_heap_elem_t* lhs,
    const ntt_timer_heap_elem_t* rhs)
{
    if (lhs->deadline != rhs->deadline) {
        return lhs->deadline < rhs->deadline ? 1 : 0;
    }
    return ntt_timer_heap_slot_at(self, lhs->index)->seq
            < ntt_timer_heap_slot_at(self, rhs->index)->seq
        ? 1
        : 0;
}

/**
 * @brief Write an entry at the given position and fix up its back reference.
 */
static inline void ntt_timer_heap_place(
    ntt_timer_heap_t* self,
    uint32_t pos,
    ntt_timer_heap_elem_t elem)
{
    *ntt_timer_heap_elem_at(self, pos)               = elem;
    ntt_timer_heap_slot_at(self, elem.index)->heap_pos = pos;
}

static void ntt_timer_heap_sift_up(
    ntt_timer_heap_t* self,
    uint32_t pos,
    ntt_timer_heap_elem_t elem)
{
    while (pos > 0) {
        uint32_t parent                = (pos - 1) / 2;
        ntt_timer_heap_elem_t parent_elem = *ntt_timer_heap_elem_at(self, parent);

        if (!ntt_timer_heap_less(self, &elem, &parent_elem)) {
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
                self,
                ntt_timer_heap_elem_at(self, right),
                ntt_timer_heap_elem_at(self, left))) {
            child = right;
        }

        ntt_timer_heap_elem_t child_elem = *ntt_timer_heap_elem_at(self, child);

        if (!ntt_timer_heap_less(self, &child_elem, &elem)) {
            break;
        }

        ntt_timer_heap_place(self, pos, child_elem);
        pos = child;
    }

    ntt_timer_heap_place(self, pos, elem);
}

/**
 * @brief Grow a block table so that it can hold cnt block pointers.
 */
static void* ntt_timer_heap_grow_table(
    void* blocks,
    uint32_t* cap,
    uint32_t cnt)
{
    if (cnt <= *cap) {
        return blocks;
    }

    uint32_t new_cap = *cap == 0 ? NTT_TIMER_BLOCK_TABLE_MIN : *cap;
    while (new_cap < cnt) {
        new_cap *= 2;
    }

    *cap = new_cap;

    return ntt_realloc_or_die(blocks, sizeof(void*) * new_cap);
}

/**
 * @brief Make sure the heap has a block for the given position.
 */
static void ntt_timer_heap_reserve_elem(ntt_timer_heap_t* self, uint32_t pos)
{
    uint32_t block = pos / NTT_TIMER_BLOCK;

    if (block < self->heap_block_cnt) {
        return;
    }

    self->heap_blocks = (ntt_timer_heap_elem_t**)ntt_timer_heap_grow_table(
        (void*)self->heap_blocks,
        &self->heap_block_cap,
        block + 1);

    while (self->heap_block_cnt <= block) {
        self->heap_blocks[self->heap_block_cnt] = ntt_malloc_or_die(
            sizeof(ntt_timer_heap_elem_t) * NTT_TIMER_BLOCK);
        self->heap_block_cnt += 1;
    }
}

/**
 * @brief Make sure the slab has a block for the given slot index.
 */
static void ntt_timer_heap_reserve_slot(ntt_timer_heap_t* self, uint32_t index)
{
    uint32_t block = index / NTT_TIMER_BLOCK;

    if (block < self->slot_block_cnt) {
        return;
    }

    self->slot_blocks = (ntt_timer_slot_t**)ntt_timer_heap_grow_table(
        (void*)self->slot_blocks,
        &self->slot_block_cap,
        block + 1);

    while (self->slot_block_cnt <= block) {
        self->slot_blocks[self->slot_block_cnt] = ntt_malloc_or_die(
            sizeof(ntt_timer_slot_t) * NTT_TIMER_BLOCK);
        self->slot_block_cnt += 1;
    }
}

/**
 * @brief Release heap blocks that are no longer needed.
 *
 * Keeps one spare block so that pushes and pops around a block boundary do
 * not allocate and free on every call. The slab is never shrunk: slot
 * indices are handles, so its entries cannot be moved.
 */
static void ntt_timer_heap_shrink(ntt_timer_heap_t* self)
{
    uint32_t used   = (self->size + NTT_TIMER_BLOCK - 1) / NTT_TIMER_BLOCK;
    uint32_t target = used + 1;

    while (self->heap_block_cnt > target) {
        self->heap_block_cnt -= 1;
        ntt_free(self->heap_blocks[self->heap_block_cnt]);
    }
}

static uint32_t ntt_timer_heap_alloc_slot(ntt_timer_heap_t* self)
{
    if (self->free_head != NTT_TIMER_SLOT_NONE) {
        uint32_t index = self->free_head;

        ntt_timer_slot_t* slot = ntt_timer_heap_slot_at(self, index);
        self->free_head        = slot->heap_pos;

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

    slot->heap_pos  = self->free_head;
    self->free_head = index;
}

/**
 * @brief Take the entry at the given position out of the heap.
 *
 * Neither releases the slot nor touches the task.
 */
static void ntt_timer_heap_remove_at(ntt_timer_heap_t* self, uint32_t pos)
{
    assert(pos < self->size);

    uint32_t last = self->size - 1;

    if (pos != last) {
        ntt_timer_heap_elem_t elem = *ntt_timer_heap_elem_at(self, last);

        self->size = last;

        if (pos > 0
            && ntt_timer_heap_less(
                self,
                &elem,
                ntt_timer_heap_elem_at(self, (pos - 1) / 2))) {
            ntt_timer_heap_sift_up(self, pos, elem);
        } else {
            ntt_timer_heap_sift_down(self, pos, elem);
        }
    } else {
        self->size = last;
    }

    ntt_timer_heap_shrink(self);
}

void ntt_timer_heap_init(ntt_timer_heap_t* self)
{
    self->heap_blocks    = NULL;
    self->heap_block_cnt = 0;
    self->heap_block_cap = 0;

    self->slot_blocks    = NULL;
    self->slot_block_cnt = 0;
    self->slot_block_cap = 0;

    self->size      = 0;
    self->slot_cnt  = 0;
    self->free_head = NTT_TIMER_SLOT_NONE;
    self->seq_next  = 0;
}

void ntt_timer_heap_deinit(ntt_timer_heap_t* self)
{
    uint32_t pos = 0;
    for (; pos < self->size; ++pos) {
        ntt_timer_heap_elem_t* elem = ntt_timer_heap_elem_at(self, pos);
        ntt_task_destroy_inl(ntt_timer_heap_slot_at(self, elem->index)->task);
    }
    self->size = 0;

    uint32_t block = 0;
    for (block = 0; block < self->heap_block_cnt; ++block) {
        ntt_free(self->heap_blocks[block]);
    }
    for (block = 0; block < self->slot_block_cnt; ++block) {
        ntt_free(self->slot_blocks[block]);
    }

    ntt_free((void*)self->heap_blocks);
    ntt_free((void*)self->slot_blocks);

    ntt_timer_heap_init(self);
}

int ntt_timer_heap_peek(const ntt_timer_heap_t* self, uint64_t* deadline)
{
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
    slot->seq              = self->seq_next;
    self->seq_next += 1;

    uint32_t pos = self->size;
    ntt_timer_heap_reserve_elem(self, pos);
    self->size = pos + 1;

    ntt_timer_heap_elem_t elem;
    elem.deadline = deadline;
    elem.index    = index;
    elem.reserved = 0;

    ntt_timer_heap_sift_up(self, pos, elem);

    ntt_timer_tag_t tag;
    tag.index   = index;
    tag.version = slot->version;

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
    uint32_t pos     = slot->heap_pos;

    ntt_timer_heap_remove_at(self, pos);
    ntt_timer_heap_free_slot(self, tag.index);

    ntt_task_destroy_inl(task);
}

size_t ntt_timer_heap_svc(ntt_timer_heap_t* self, uint64_t now)
{
    size_t cnt = 0;

    for (;;) {
        if (self->size == 0) {
            break;
        }

        ntt_timer_heap_elem_t* top = ntt_timer_heap_elem_at(self, 0);

        if (top->deadline > now) {
            break;
        }

        uint32_t index   = top->index;
        ntt_task_t* task = ntt_timer_heap_slot_at(self, index)->task;

        // The timer is retired before its task runs, so the task is free to
        // arm new timers and to cancel other armed ones.
        ntt_timer_heap_remove_at(self, 0);
        ntt_timer_heap_free_slot(self, index);

        ntt_task_svc_inl(task);
        ntt_task_destroy_inl(task);

        cnt += 1;
    }

    return cnt;
}
