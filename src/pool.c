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
    ntt_pool_stopped_cb* stopped_cb = self->stopped_cb;
    void* stopped_ctx = self->stopped_ctx;

    pthread_spin_destroy(&self->tasks_lock);

    int rc = epoll_ctl(self->epoll_fd, EPOLL_CTL_DEL, self->eventfd, NULL);
    assert(rc == 0 && "ntt pool failed to del internal event fd");

    rc = close(self->epoll_fd);
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

    stopped_cb(stopped_ctx);
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

ntt_pool_t* ntt_pool_create_impl(
    unsigned short width,
    ntt_pool_stopped_cb* stopped_cb,
    void* stopped_ctx)
{
    int rc;
    int i;

    ntt_pool_t* self = malloc(sizeof(ntt_pool_t) + sizeof(ntt_thread_t) * width);
    assert(self != NULL);

    self->external_refs = 1;
    self->internal_refs = width;

    self->stopped_cb = stopped_cb;
    self->stopped_ctx = stopped_ctx;

    self->thread_cnt = width;

    self->eventfd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE);
    assert(self->eventfd != -1);

    self->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    assert(self->epoll_fd != -1);

    struct epoll_event event;
    event.data.fd = self->eventfd;
    event.events = EPOLLIN | EPOLLET;
    rc = epoll_ctl(self->epoll_fd, EPOLL_CTL_ADD, self->eventfd, &event);
    assert(rc == 0);

    // init combined queue
    ntt_task_list_init(&self->tasks);
    self->task_awaiters = width;
    pthread_spin_init(&self->tasks_lock, PTHREAD_PROCESS_PRIVATE);

    // init task cache
    // self->task_cache = ntt_task_cache_create();
    self->task_cache = NULL;

    for (i = 0; i < width; ++i) {
        ntt_thread_init(&self->threads[i], ntt_pool_thread_stopped, self,
            self->epoll_fd);
    }
    return self;
}

ntt_pool_t* ntt_pool_create(
    unsigned short width)
{
    return ntt_pool_create_impl(width, NULL, NULL);
}

ntt_pool_t* ntt_pool_with_stopped_cb(
    unsigned short width,
    ntt_pool_stopped_cb* stopped_cb,
    void* stopped_ctx)
{
    return ntt_pool_create_impl(width, stopped_cb, stopped_ctx);
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

ntt_pool_t* ntt_pool_acquire(ntt_pool_t* self)
{
    size_t prev = atomic_fetch_add(&self->external_refs, 1);
    assert(prev > 0 && "ntt pool already deleted");
    return self;
}

void ntt_pool_release(ntt_pool_t* self)
{
    size_t prev = atomic_fetch_sub(&self->external_refs, 1);
    assert(prev > 0 && "ntt pool already deleted");
    if (prev == 1) {
        ntt_pool_stop(self);
    }
}

typedef struct ntt_barrier_task {
    atomic_size_t refs;
    ntt_task_t* task;
    ntt_task_node_t task_nodes[];
} barrier_task_t;

void ntt_barrier_task_svc(void* ctx)
{
    ntt_task_node_t* task_node = ctx;

    struct ntt_barrier_task* self = *(struct ntt_barrier_task**)task_node->payload;

    if (atomic_fetch_sub(&self->refs, 1) != 1) {
        return;
    }

    ntt_do_task_inl(self->task);
    ntt_free_task(self->task);
    free(self);
}

void ntt_barrier_dummy_call(void* ctx)
{
}

void ntt_pool_post_barrier_task(ntt_pool_t* self, ntt_task_t* task)
{
    struct ntt_barrier_task* barrier = malloc(sizeof(struct ntt_barrier_task) + sizeof(ntt_task_node_t) * self->thread_cnt);
    barrier->task = task;
    barrier->refs = self->thread_cnt;
    unsigned int i;
    for (i = 0; i < self->thread_cnt; ++i) {
        ntt_task_t* nth_task = ntt_task_init(&barrier->task_nodes[i], ntt_barrier_dummy_call, ntt_barrier_task_svc);
        *(struct ntt_barrier_task**)nth_task = barrier;
        ntt_thread_send_task(&self->threads[i], nth_task);
    }
}
