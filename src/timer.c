#include "./queue.h"
#include "./task.h"
#include "./timer_heap.h"
#include "./timer_service.h"
#include "ntt/timer.h"

#include <assert.h>

ntt_timer_tag_t ntt_self_wakeup_at(uint64_t deadline, ntt_task_t* task)
{
    ntt_queue_t* queue = ntt_queue_self();

    assert(queue != NULL && "[ntt_self_wakeup_at] not in a queue context");
    assert(
        ntt_queue_timer_service(queue) != NULL
        && "[ntt_self_wakeup_at] the queue has no timer service");

    ntt_timer_tag_t tag = ntt_timer_heap_push(
        ntt_queue_timers(queue),
        deadline,
        task);

    ntt_queue_sync_timers(queue);

    return tag;
}

ntt_timer_tag_t ntt_self_wakeup_after(uint64_t delay, ntt_task_t* task)
{
    return ntt_self_wakeup_at(ntt_now_ns() + delay, task);
}

void ntt_self_cancel_timer(ntt_timer_tag_t tag)
{
    ntt_queue_t* queue = ntt_queue_self();

    assert(queue != NULL && "[ntt_self_cancel_timer] not in a queue context");

    ntt_timer_heap_cancel(ntt_queue_timers(queue), tag);

    ntt_queue_sync_timers(queue);
}
