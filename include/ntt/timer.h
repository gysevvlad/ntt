#pragma once

#include "ntt/defs.h"
#include "ntt/task.h"

EXTERN_START

#include <stdint.h>

/**
 * @brief Timer handle.
 *
 * Returned when a timer is armed and consumed when it is cancelled. The
 * handle is single use: once the timer has fired or has been cancelled it is
 * stale and must not be used again.
 */
typedef struct ntt_timer_tag {
    uint32_t index;
    uint32_t version;
} ntt_timer_tag_t;

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
 * @brief Current time of the clock the timers run on, in nanoseconds.
 *
 * CLOCK_MONOTONIC, read through the vDSO, so this is not a system call.
 */
NTT_EXPORT uint64_t ntt_now_ns();

/**
 * @brief Arm a timer in the current queue.
 *
 * @param[in] deadline Absolute deadline on the ntt_now_ns clock.
 * @param[in] task Task to run once the deadline passes. Ownership is taken.
 *
 * @return The tag of the armed timer, never null.
 *
 * @pre Called from a queue context, and the queue has a timer service.
 */
NTT_EXPORT ntt_timer_tag_t ntt_self_wakeup_at(
    uint64_t deadline,
    ntt_task_t* task);

/**
 * @brief Arm a timer in the current queue, relative to now.
 */
NTT_EXPORT ntt_timer_tag_t ntt_self_wakeup_after(
    uint64_t delay,
    ntt_task_t* task);

/**
 * @brief Cancel a timer of the current queue.
 *
 * Destroys the task of the timer without running it. After the call the timer
 * cannot fire in this queue.
 *
 * @pre Called from the queue context the tag was obtained in.
 * @pre The timer has neither fired nor been cancelled before.
 */
NTT_EXPORT void ntt_self_cancel_timer(ntt_timer_tag_t tag);

EXTERN_STOP
