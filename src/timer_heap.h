#pragma once

#include "ntt/defs.h"
#include "ntt/task.h"

EXTERN_START

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Number of entries in one timer storage block.
 *
 * Both the heap and the slab grow by whole blocks, so entries are never
 * moved by a reallocation. Must be a power of two.
 */
#ifndef NTT_TIMER_BLOCK
#define NTT_TIMER_BLOCK 1024
#endif

NTT_STATIC_ASSERT(
    NTT_TIMER_BLOCK > 0 && (NTT_TIMER_BLOCK & (NTT_TIMER_BLOCK - 1)) == 0,
    "ntt exception: NTT_TIMER_BLOCK must be a power of two");

/**
 * @brief Sentinel for an absent slot index.
 */
#define NTT_TIMER_SLOT_NONE UINT32_MAX

/**
 * @brief Timer handle.
 *
 * Returned by ntt_timer_heap_push and consumed by ntt_timer_heap_cancel.
 * The handle is single-use: once the timer has fired or has been cancelled
 * the handle is stale and must not be used again.
 */
typedef struct ntt_timer_tag {
    uint32_t index;
    uint32_t version;
} ntt_timer_tag_t;

/**
 * @brief Heap entry.
 *
 * The deadline is kept inline so that sifting compares contiguous memory.
 */
typedef struct ntt_timer_heap_elem {
    uint64_t deadline;
    uint32_t index;
    uint32_t reserved;
} ntt_timer_heap_elem_t;

/**
 * @brief Slab entry.
 *
 * While occupied it holds the task, its sequence number and its position in
 * the heap. While vacant the heap_pos field links the free list.
 *
 * The version is odd while the slot is occupied and even while it is vacant,
 * so parity alone tells the two states apart. It is bumped on every release,
 * which makes a stale tag detectable.
 */
typedef struct ntt_timer_slot {
    ntt_task_t* task;
    uint64_t seq;
    uint32_t heap_pos;
    uint32_t version;
} ntt_timer_slot_t;

/**
 * @brief Timer heap of a single queue.
 *
 * A binary min-heap ordered by (deadline, seq) on top of a slab of timer
 * slots. Ordering is stable: timers with equal deadlines fire in the order
 * they were started.
 *
 * The structure is not thread safe by design. All operations are expected to
 * run in the serial context of the owning queue.
 */
typedef struct ntt_timer_heap {
    ntt_timer_heap_elem_t** heap_blocks;
    uint32_t heap_block_cnt;
    uint32_t heap_block_cap;

    ntt_timer_slot_t** slot_blocks;
    uint32_t slot_block_cnt;
    uint32_t slot_block_cap;

    /** Number of entries in the heap. */
    uint32_t size;

    /** Number of slots ever handed out, i.e. the slab watermark. */
    uint32_t slot_cnt;

    /** Head of the free slot list or NTT_TIMER_SLOT_NONE. */
    uint32_t free_head;

    /** Sequence number for the next started timer. */
    uint64_t seq_next;
} ntt_timer_heap_t;

/**
 * @brief Make a tag that refers to no timer.
 */
static inline ntt_timer_tag_t ntt_timer_tag_null()
{
    ntt_timer_tag_t tag = { 0, 0 };
    return tag;
}

/**
 * @brief Check that a tag refers to no timer.
 *
 * @return Returns 1 for a null tag, otherwise 0.
 */
static inline int ntt_timer_tag_is_null(ntt_timer_tag_t tag)
{
    return tag.version == 0 ? 1 : 0;
}

/**
 * @brief Initialize an empty timer heap.
 *
 * Does not allocate. The first pushed timer allocates one block of each kind.
 */
void ntt_timer_heap_init(ntt_timer_heap_t* self);

/**
 * @brief Release the timer heap.
 *
 * Destroys the tasks of all timers that are still armed without running them.
 */
void ntt_timer_heap_deinit(ntt_timer_heap_t* self);

/**
 * @brief Number of armed timers.
 */
static inline size_t ntt_timer_heap_size(const ntt_timer_heap_t* self)
{
    return self->size;
}

/**
 * @brief Check that no timer is armed.
 *
 * @return Returns 1 if the heap is empty, otherwise 0.
 */
static inline int ntt_timer_heap_empty(const ntt_timer_heap_t* self)
{
    return self->size == 0 ? 1 : 0;
}

/**
 * @brief Read the earliest deadline.
 *
 * @param[out] deadline Set to the earliest deadline when the heap is not
 *                      empty, left untouched otherwise.
 *
 * @return Returns 1 if a deadline was written, otherwise 0.
 */
int ntt_timer_heap_peek(const ntt_timer_heap_t* self, uint64_t* deadline);

/**
 * @brief Arm a timer.
 *
 * @param[in] deadline Absolute deadline.
 * @param[in] task Task to run when the deadline passes. Ownership is taken:
 *                 the heap runs and destroys it, or destroys it on cancel.
 *
 * @return The tag of the armed timer, never null.
 *
 * @pre task != NULL && ntt_task_unlinked(task) == 1
 */
ntt_timer_tag_t ntt_timer_heap_push(
    ntt_timer_heap_t* self,
    uint64_t deadline,
    ntt_task_t* task);

/**
 * @brief Cancel an armed timer.
 *
 * Destroys the task of the timer without running it. After the call the
 * timer cannot fire.
 *
 * @pre The tag was returned by ntt_timer_heap_push of this heap.
 * @pre The timer has neither fired nor been cancelled before.
 *
 * Violating the preconditions is a contract error: it trips an assert in a
 * debug build and is ignored in a release build.
 */
void ntt_timer_heap_cancel(ntt_timer_heap_t* self, ntt_timer_tag_t tag);

/**
 * @brief Run every timer whose deadline is not later than now.
 *
 * Timers are taken off the heap one at a time, so the task of an expired
 * timer may cancel another expired timer and that one will not run. A task
 * that arms a new timer with a deadline of now or earlier will see it run
 * within the same call.
 *
 * @param[in] now Current time.
 *
 * @return Number of timers that ran.
 */
size_t ntt_timer_heap_svc(ntt_timer_heap_t* self, uint64_t now);

EXTERN_STOP
