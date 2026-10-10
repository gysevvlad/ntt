#include "./timer_service.h"

#include "./die.h"
#include "./loop.h"
#include "./malloc.h"
#include "./queue.h"
#include "./queue_heap.h"
#include "./task.h"
#include "ntt/impl/atomic.h"

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

/**
 * @brief How many expired queues are taken out of the heap under one lock.
 *
 * Waking a queue allocates a task and takes the locks of the queue and of the
 * combine, so it is done with the service lock released.
 */
#define NTT_TIMER_SERVICE_BATCH 16

uint64_t ntt_now_ns()
{
    struct timespec ts;

    // vDSO, not a system call
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;
}

/**
 * @brief Arm the timerfd at the earliest deadline, if that is earlier than now.
 *
 * @pre The lock is held.
 */
static void ntt_timer_service_arm_locked(ntt_timer_service_t* self)
{
    uint64_t deadline = 0;

    if (ntt_queue_heap_top(&self->queues, &deadline) == 0) {
        return;
    }

    if (deadline >= self->armed) {
        return;
    }

    struct itimerspec spec;
    spec.it_interval.tv_sec  = 0;
    spec.it_interval.tv_nsec = 0;
    spec.it_value.tv_sec     = (time_t)(deadline / 1000000000ULL);
    spec.it_value.tv_nsec    = (long)(deadline % 1000000000ULL);

    // An it_value of zero would disarm the timer, so ask for the next
    // nanosecond instead, which is in the past anyway and fires at once.
    if (spec.it_value.tv_sec == 0 && spec.it_value.tv_nsec == 0) {
        spec.it_value.tv_nsec = 1;
    }

    if (timerfd_settime(self->fd, TFD_TIMER_ABSTIME, &spec, NULL) != 0) {
        ntt_die();
    }

    self->armed = deadline;
}

void ntt_timer_service_register(
    ntt_timer_service_t* self,
    ntt_queue_t* queue,
    uint64_t deadline)
{
    pthread_spin_lock(&self->lock);

    if (queue->timer_pos == NTT_QUEUE_HEAP_NONE) {
        // The reference is taken inside the critical section on purpose: the
        // moment the queue is in the heap, a worker handling a tick may take
        // it out again and release exactly this reference. Taking it after the
        // unlock would let that release run first and destroy the queue.
        ntt_queue_acquire(queue);
        ntt_queue_heap_insert(&self->queues, queue, deadline);
    } else if (deadline < self->queues.elems[queue->timer_pos].deadline) {
        ntt_queue_heap_decrease(&self->queues, queue, deadline);
    } else {
        deadline = self->queues.elems[queue->timer_pos].deadline;
    }

    atomic_store_explicit(&queue->timer_key, deadline, memory_order_relaxed);

    ntt_timer_service_arm_locked(self);

    pthread_spin_unlock(&self->lock);
}

void ntt_timer_service_deregister(
    ntt_timer_service_t* self,
    ntt_queue_t* queue)
{
    int linked = 0;

    pthread_spin_lock(&self->lock);

    if (queue->timer_pos != NTT_QUEUE_HEAP_NONE) {
        ntt_queue_heap_remove(&self->queues, queue);
        linked = 1;
    }

    atomic_store_explicit(&queue->timer_key, UINT64_MAX, memory_order_relaxed);

    pthread_spin_unlock(&self->lock);

    if (linked) {
        ntt_queue_release(queue);
    }
}

/**
 * @brief Timerfd became readable: wake every queue whose deadline has passed.
 *
 * May run on several workers at once, since the timerfd is edge triggered and
 * the event may be delivered again while this is still running. That is safe:
 * the state is behind the lock, and a worker that finds nothing expired just
 * leaves.
 */
static void ntt_timer_service_ready(
    ntt_epoll_event_t* event,
    void* ctx,
    uint32_t events)
{
    (void)event;
    (void)events;

    ntt_timer_service_t* self = ctx;

    // The timerfd has to be read: it stays readable otherwise, and an edge
    // triggered descriptor that is never drained produces no further events.
    uint64_t expirations = 0;
    if (read(self->fd, &expirations, sizeof(expirations)) < 0) {
        if (errno != EAGAIN) {
            ntt_die();
        }
    }

    for (;;) {
        ntt_queue_t* batch[NTT_TIMER_SERVICE_BATCH];
        unsigned cnt = 0;

        uint64_t now = ntt_now_ns();

        pthread_spin_lock(&self->lock);

        while (cnt < NTT_TIMER_SERVICE_BATCH) {
            ntt_queue_t* queue = ntt_queue_heap_take_expired(&self->queues, now);

            if (queue == NULL) {
                break;
            }

            atomic_store_explicit(&queue->timer_key, UINT64_MAX, memory_order_relaxed);
            batch[cnt] = queue;
            cnt += 1;
        }

        // The timerfd has fired, so it is disarmed. Whatever is left on top is
        // not later than any deadline registered meanwhile, so arming at it is
        // always correct.
        self->armed = UINT64_MAX;
        ntt_timer_service_arm_locked(self);

        pthread_spin_unlock(&self->lock);

        unsigned i = 0;
        for (; i < cnt; ++i) {
            ntt_queue_wakeup_timers(batch[i]);

            // The reference the heap held, now that the queue holds the task.
            ntt_queue_release(batch[i]);
        }

        if (cnt < NTT_TIMER_SERVICE_BATCH) {
            break;
        }
    }
}

static void ntt_timer_service_cancelled(ntt_epoll_event_t* event, void* ctx)
{
    (void)event;
    (void)ctx;
}

const static ntt_epoll_event_vtbl_t ntt_g_timer_service_vtbl = {
    .name      = "timer_service",
    .ready     = ntt_timer_service_ready,
    .cancelled = ntt_timer_service_cancelled,
};

void ntt_timer_service_init(ntt_timer_service_t* self, ntt_loop_t* loop)
{
    pthread_spin_init(&self->lock, 0);
    ntt_queue_heap_init(&self->queues);
    self->armed = UINT64_MAX;
    self->loop  = loop;

    self->fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (self->fd == -1) {
        ntt_die();
    }

    ntt_epoll_event_init(&self->event, &ntt_g_timer_service_vtbl, self, 0);
    self->event.fd = self->fd;

    // Edge triggered, so one expiration gives one event, and exclusive, so
    // the kernel wakes one worker instead of all of them. The service is not
    // work of the loop: queues with armed timers keep the loop alive.
    struct epoll_event event;
    event.events   = EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
    event.data.ptr = &self->event;

    ntt_loop_add_internal_epoll_event(loop, self->fd, &event);
}

void ntt_timer_service_deinit(ntt_timer_service_t* self)
{
    assert(ntt_queue_heap_empty(&self->queues) == 1);

    // Closing the descriptor removes it from the epoll of the loop.
    close(self->fd);

    ntt_queue_heap_deinit(&self->queues);
    pthread_spin_destroy(&self->lock);
}
