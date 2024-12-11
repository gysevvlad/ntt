#include "ntt/pool.h"
#include "ntt/impl/list.h"
#include "ntt/impl/ntt_task_cache.h"
#include "ntt/impl/pool.h"
#include "ntt/impl/task.h"
#include "ntt/impl/thread.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/syscall.h>

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SIGNTTACTION SIGRTMIN + 16

void ntt_pool_destroy(ntt_pool_t* self)
{
    pthread_spin_destroy(&self->tasks_lock);

    int rc = epoll_ctl(self->epollfd, EPOLL_CTL_DEL, self->eventfd, NULL);
    assert(rc == 0 && "ntt pool failed to del internal event fd");

    rc = close(self->epollfd);
    assert(rc == 0 && "ntt pool failed to close internal event fd");

    rc = close(self->eventfd);
    assert(rc == 0 && "ntt pool failed to close epoll fd");

    // TODO: support external stop slot
    size_t i;
    for (i = 0; i < self->thread_cnt; ++i) {
        pthread_detach(self->threads[i].id);
    }

    ntt_task_cache_destroy(self->task_cache);

    free(self);
}

void ntt_pool_release_internal(ntt_pool_t* self)
{
    assert(atomic_load_explicit(&self->external_refs, memory_order_relaxed) == 0 && "ntt pool has unexpected external refs");
    size_t prev = atomic_fetch_sub(&self->internal_refs, 1);
    assert(prev > 0 && "ntt pool has unexpected internal refs");
    if (prev == 1) {
        ntt_pool_destroy(self);
    }
}

void ntt_pool_post_task(ntt_pool_t* self, ntt_task_t* task)
{
    int need_wakeup = 0;
    pthread_spin_lock(&self->tasks_lock);
    int first;
    ntt_task_list_push(&self->tasks, task, &first);
    if (self->task_awaiters > 0) {
        self->task_awaiters -= 1;
        need_wakeup = 1;
    }
    pthread_spin_unlock(&self->tasks_lock);
    if (need_wakeup) {
        eventfd_write(self->eventfd, 1);
    }
}

void ntt_pool_thread_stopped(ntt_thread_t* thread, void* context)
{
    ntt_pool_release_internal(context);
}

ntt_pool_t* ntt_pool_create(unsigned short width)
{
    int rc;
    int i;

    ntt_pool_t* self = malloc(sizeof(ntt_pool_t) + sizeof(ntt_thread_t) * width);
    assert(self != NULL);

    self->external_refs = 1;
    self->internal_refs = width;

    self->thread_cnt = width;

    self->eventfd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE);
    assert(self->eventfd != -1);

    self->epollfd = epoll_create1(EPOLL_CLOEXEC);
    assert(self->epollfd != -1);

    struct epoll_event event;
    event.data.fd = self->eventfd;
    event.events = EPOLLIN | EPOLLET;
    rc = epoll_ctl(self->epollfd, EPOLL_CTL_ADD, self->eventfd, &event);
    assert(rc == 0);

    // init combined queue
    ntt_task_list_init(&self->tasks);
    self->task_awaiters = width;
    pthread_spin_init(&self->tasks_lock, PTHREAD_PROCESS_PRIVATE);

    // init task cache
    self->task_cache = ntt_task_cache_create();

    for (i = 0; i < width; ++i) {
        ntt_thread_init(&self->threads[i], ntt_pool_thread_stopped, self,
            self->epollfd);
    }
    return self;
}

void ntt_pool_send_task(ntt_pool_t* pool, unsigned short idx,
    ntt_task_t* task)
{
    ntt_thread_send_task(&pool->threads[idx], task);
}

ntt_task_t* ntt_pool_alloc_task(ntt_pool_t* pool, ntt_task_cb_t* task_cb)
{
    return ntt_task_cache_alloc_task(pool->task_cache, task_cb);
}

void ntt_pool_stop(ntt_pool_t* self)
{
    unsigned int i;
    for (i = 0; i < self->thread_cnt; ++i) {
        ntt_thread_send_stop(&self->threads[i]);
    }
}

void ntt_pool_acquire(ntt_pool_t* self)
{
    size_t prev = atomic_fetch_add(&self->external_refs, 1);
    assert(prev > 0 && "ntt pool already deleted");
}

void ntt_pool_release(ntt_pool_t* self)
{
    size_t prev = atomic_fetch_sub(&self->external_refs, 1);
    assert(prev > 0 && "ntt pool already deleted");
    if (prev == 1) {
        ntt_pool_stop(self);
    }
}
