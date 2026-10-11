#include "./timer_service.h"

#include "./die.h"
#include "./loop.h"
#include "./malloc.h"
#include "./queue.h"
#include "./queue_heap.h"
#include "./slab.h"
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
    uint32_t index,
    uint32_t generation,
    uint64_t deadline,
    uint32_t tombstones)
{
    pthread_spin_lock(&self->lock);

    ntt_queue_heap_push(&self->queues, index, generation, deadline);

    if (tombstones != 0) {
        ntt_queue_heap_retire(
            &self->queues,
            ntt_loop_queue_slab(self->loop),
            tombstones);
    }

    ntt_timer_service_arm_locked(self);

    pthread_spin_unlock(&self->lock);
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

    const ntt_slab_t* slab = ntt_loop_queue_slab(self->loop);

    for (;;) {
        ntt_queue_t* batch[NTT_TIMER_SERVICE_BATCH];
        unsigned cnt = 0;

        uint64_t now = ntt_now_ns();

        pthread_spin_lock(&self->lock);

        while (cnt < NTT_TIMER_SERVICE_BATCH) {
            ntt_queue_heap_elem_t elem;

            if (ntt_queue_heap_take_expired(&self->queues, slab, now, &elem) == 0) {
                break;
            }

            ntt_queue_t* queue = ntt_slab_at(slab, elem.index);

            // The storage of a queue outlives the queue itself, so take a
            // reference first and only then trust the generation: without the
            // reference the queue could be destroyed between the two.
            if (ntt_queue_acquire_if_live(queue) == 0) {
                continue;
            }

            if (ntt_queue_timer_generation(queue) != elem.generation) {
                ntt_queue_release(queue);
                continue;
            }

            // The entry is consumed, so the registration is gone. Bumping the
            // generation is what tells the queue context to register again.
            ntt_queue_retire_timer_generation(queue);

            batch[cnt] = queue;
            cnt += 1;
        }

        self->armed = UINT64_MAX;
        ntt_timer_service_arm_locked(self);

        pthread_spin_unlock(&self->lock);

        unsigned i = 0;
        for (; i < cnt; ++i) {
            ntt_queue_wakeup_timers(batch[i]);

            // The reference taken above, now that the queue holds the task.
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
    assert(
        ntt_queue_heap_has_live(&self->queues, ntt_loop_queue_slab(self->loop)) == 0
        && "[ntt_timer_service] queues are still registered");

    // Closing the descriptor removes it from the epoll of the loop.
    close(self->fd);

    ntt_queue_heap_deinit(&self->queues);
    pthread_spin_destroy(&self->lock);
}
