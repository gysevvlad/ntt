#include "ntt/impl/loop.h"
#include "ntt/impl/epoll_event.h"
#include "ntt/impl/worker.h"
#include "ntt/sigset.h"
#include "ntt/task.h"
#include "ntt/worker.h"

#include <bits/types/sigset_t.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>

static void ntt_loop_leader_enter(void* ctx, ntt_worker_t* worker)
{
    ntt_loop_t* self = ctx;

    self->workers[self->width - 1].worker = ntt_worker_acquire(worker);

    pthread_mutex_lock(&self->mtx);
    while (self->followers_ready < self->width - 1) {
        pthread_cond_wait(&self->cnd, &self->mtx);
    }
    pthread_mutex_unlock(&self->mtx);

    self->work_cnt = 1;
    self->cbs.on_start(self, self->ctx);
    ntt_loop_work_leave(self);
}

static void ntt_loop_follower_enter(void* ctx, ntt_worker_t* worker)
{
    ntt_loop_t* self = ctx;

    pthread_mutex_lock(&self->mtx);
    while (!self->followers_created) {
        pthread_cond_wait(&self->cnd, &self->mtx);
    }

    unsigned i = 0;
    for (; i < self->width - 1; ++i) {
        if (pthread_equal(pthread_self(), self->workers[i].thread_id)) {
            self->workers[i].worker = ntt_worker_acquire(worker);
        }
    }
    self->followers_ready += 1;
    pthread_cond_signal(&self->cnd);
    pthread_mutex_unlock(&self->mtx);
}

static void ntt_loop_worker_leave(void* ctx)
{
    // TODO: ...
}

static void ntt_loop_worker_svc(void* ctx, ntt_sigset_t* sigset)
{
    ntt_loop_t* self = ctx;

    struct epoll_event event;

    int rc = 0;

repeat:
    rc = epoll_pwait(
        self->epoll_fd,
        &event,
        1,
        -1,
        (sigset_t*)sigset);

    if ntt_likely (rc <= 0) {
        return;
    }

    ntt_epoll_event_ready(&event);
    goto repeat;
}

static void ntt_loop_leader_worker_svc(void* ctx, ntt_sigset_t* sigset)
{
    ntt_loop_t* self = ctx;

    struct epoll_event event;

    int rc = 0;

repeat:
    rc = epoll_pwait(
        self->epoll_fd,
        &event,
        1,
        -1,
        (sigset_t*)sigset);

    if ntt_likely (rc <= 0) {
        if (self->got_sighup != 0) {
            self->got_sighup = 0;
            self->cbs.on_signal(self, self->ctx, SIGHUP);
        }
        if (self->got_sigint != 0) {
            self->got_sigint = 0;
            self->cbs.on_signal(self, self->ctx, SIGINT);
        }
        if (self->got_sigterm != 0) {
            self->got_sigterm = 0;
            self->cbs.on_signal(self, self->ctx, SIGTERM);
        }
        return;
    }

    ntt_epoll_event_ready(&event);
    goto repeat;
}

const static ntt_worker_cbs_t g_ntt_loop_follower_cbs = {
    .enter_cb = ntt_loop_follower_enter,
    .leave_cb = ntt_loop_worker_leave,
    .svc_cb   = ntt_loop_worker_svc,
};

const static ntt_worker_cbs_t g_ntt_loop_leader_cbs = {
    .enter_cb = ntt_loop_leader_enter,
    .leave_cb = ntt_loop_worker_leave,
    .svc_cb   = ntt_loop_leader_worker_svc,
};

static void* ntt_loop_thread_svc(void* ctx)
{
    ntt_worker_svc(g_ntt_loop_follower_cbs, ctx);
    return NULL;
}

static int ntt_epoll_create1_or_abort(int flags)
{
    int fd = epoll_create1(flags);
    if (fd == -1) {
        abort();
    }
    return fd;
}

static ntt_loop_t* g_main_loop = NULL;

void ntt_handle_signal(int sig)
{
    switch (sig) {
    case SIGTERM:
        g_main_loop->got_sigterm = 1;
        break;
    case SIGINT:
        g_main_loop->got_sigint = 1;
        break;
    case SIGHUP:
        g_main_loop->got_sighup = 1;
        break;
    default:
        break;
    }
}

int ntt_loop_svc(ntt_loop_cbs_t cbs, void* ctx, unsigned width)
{
    if (width == 0) {
        return -1;
    }

    // TODO(vgusev): limit width
    // TODO(vgusev): place ntt_loop_t and related on stack
    ntt_loop_t* self = malloc(sizeof(ntt_loop_t) + sizeof(ntt_loop_thread_ctx_t) * width);

    if (self == NULL) {
        return -1;
    }

    self->cbs      = cbs;
    self->ctx      = ctx;
    self->epoll_fd = ntt_epoll_create1_or_abort(EPOLL_CLOEXEC);
    pthread_mutex_init(&self->mtx, NULL);
    self->width             = width;
    self->followers_created = 0;
    self->followers_ready   = 0;
    self->got_sigint        = 0;
    self->got_sighup        = 0;
    self->got_sigterm       = 0;
    pthread_cond_init(&self->cnd, NULL);
    self->work_cnt = 0;

    sigset_t block;
    sigemptyset(&block);
    sigaddset(&block, SIGINT);
    sigaddset(&block, SIGTERM);
    sigaddset(&block, SIGHUP);

    ntt_sigset_t origin_mask;
    sigemptyset((sigset_t*)origin_mask.data);

    // Block signals and save origin_mask
    pthread_sigmask(SIG_BLOCK, &block, (sigset_t*)origin_mask.data);

    g_main_loop = self;

    struct sigaction sa = { 0 };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags   = SA_RESTART;
    sa.sa_handler = ntt_handle_signal;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    pthread_mutex_lock(&self->mtx);
    unsigned i = 0;
    for (i = 0; i < width - 1; ++i) {
        pthread_create(&self->workers[i].thread_id, NULL, ntt_loop_thread_svc, self);
    }
    self->workers[width - 1].thread_id = pthread_self();
    self->followers_created            = 1;
    pthread_cond_broadcast(&self->cnd);
    pthread_mutex_unlock(&self->mtx);

    ntt_worker_svc_with_mask(g_ntt_loop_leader_cbs, origin_mask, self);

    for (i = 0; i < width; ++i) {
        if (!pthread_equal(pthread_self(), self->workers[i].thread_id)) {
            pthread_join(self->workers[i].thread_id, NULL);
        }
    }

    ntt_loop_destroy(self);
    free(self);

    return 0;
}

void ntt_loop_destroy(ntt_loop_t* self)
{
    // self->vptr = NULL;
    // self->ctx = NULL;
    close(self->epoll_fd);
    pthread_mutex_destroy(&self->mtx);
    // self->width = 0;
    // self->followers_created = 0;
    // self->followers_ready = 0;
    pthread_cond_destroy(&self->cnd);
    // self->active = 0;
}

ntt_loop_t* ntt_loop_acquire(ntt_loop_t* self)
{
    if (self != NULL) {
        ntt_loop_work_enter(self);
    }
    return self;
}

void ntt_loop_release(ntt_loop_t* self)
{
    if (self != NULL) {
        ntt_loop_work_leave(self);
    }
}

void ntt_loop_work_enter(ntt_loop_t* self)
{
    size_t prev = atomic_fetch_add(&self->work_cnt, 1);
    assert(prev > 0);
}

void ntt_loop_work_leave(ntt_loop_t* self)
{
    size_t prev = atomic_fetch_sub(&self->work_cnt, 1);
    assert(prev > 0);
    if (prev == 1) {
        unsigned i = 0;
        for (; i < self->width; ++i) {
            ntt_worker_release(self->workers[i].worker);
        }
    }
}

typedef struct ntt_barrier_task {
    atomic_size_t refs;
    ntt_task_t* task;
    ntt_task_node_t task_nodes[];
} ntt_barrier_task_t;

static void ntt_loop_barrier_task_svc(void* ctx)
{
    ntt_task_node_t* task_node = ctx;

    ntt_barrier_task_t* self = *(ntt_barrier_task_t**)task_node->payload;

    if (atomic_fetch_sub(&self->refs, 1) != 1) {
        return;
    }

    ntt_do_task_inl(self->task);
    ntt_free_task(self->task);
    free(self);
}

static void ntt_loop_barrier_dummy_call(void* ctx)
{
}

static void ntt_loop_post_barrier_task(ntt_loop_t* self, ntt_task_t* task)
{
    struct ntt_barrier_task* barrier = malloc(sizeof(struct ntt_barrier_task) + sizeof(ntt_task_node_t) * self->width);
    barrier->task                    = task;
    barrier->refs                    = self->width;
    unsigned i                       = 0;
    for (; i < self->width; ++i) {
        ntt_task_t* nth_task                 = ntt_task_init(&barrier->task_nodes[i], ntt_loop_barrier_dummy_call, ntt_loop_barrier_task_svc);
        *(struct ntt_barrier_task**)nth_task = barrier;
        ntt_worker_push_task(self->workers[i].worker, nth_task);
    }
}

static void ntt_loop_epoll_event_canceled(void* payload)
{
    ntt_epoll_event_t* self = *(ntt_epoll_event_t**)(payload);
    ntt_loop_t* loop        = self->loop;
    ntt_epoll_event_cancelled(self);
    ntt_loop_work_leave(loop);
}

int ntt_loop_add_epoll_event(ntt_loop_t* self, ntt_epoll_event_t* epoll_event)
{
    struct epoll_event event;
    event.data.ptr    = epoll_event;
    event.events      = epoll_event->events;
    epoll_event->loop = self;
    int rc            = epoll_ctl(self->epoll_fd, EPOLL_CTL_ADD, epoll_event->fd, &event);
    if (rc != 0) {
        return errno;
    }
    ntt_loop_work_enter(self);
    return 0;
}

int ntt_loop_del_epoll_event(ntt_loop_t* self, ntt_epoll_event_t* epoll_event)
{
    int rc = epoll_ctl(self->epoll_fd, EPOLL_CTL_DEL, epoll_event->fd, NULL);
    if (rc == -1) {
        return errno;
    }
    ntt_task_t* task           = ntt_make_task(ntt_loop_epoll_event_canceled, NULL);
    *(ntt_epoll_event_t**)task = epoll_event;
    ntt_loop_post_barrier_task(self, task);
    return 0;
}
