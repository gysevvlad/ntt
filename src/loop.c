#include "./loop.h"
#include "./signal.h"
#include "./worker.h"
#include "combine.h"
#include "ntt/impl/epoll_event.h"
#include "ntt/queue.h"
#include "ntt/task.h"
#include "queue.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <threads.h>

static ntt_loop_t* g_main_loop = NULL;

static thread_local ntt_loop_t* t_loop = NULL;

void ntt_loop_follower_enter(void* ctx, ntt_worker_t* worker)
{
    ntt_loop_worker_ctx_t* context = ctx;

    context->worker = ntt_worker_acquire(worker);

    ntt_loop_t* self = context->loop;

    pthread_mutex_lock(&self->mtx);
    self->enter_cnt += 1;
    pthread_cond_signal(&self->cv);
    pthread_mutex_unlock(&self->mtx);

    t_loop = self;
}

void ntt_loop_drain_combine(ntt_loop_t* self)
{
    for (;;) {
        ntt_queue_t* queue = ntt_combine_try_take(&self->combine);

        if (queue == NULL) {
            break;
        }

        ntt_queue_svc(queue);
    }
}

void ntt_loop_svc(void* ctx, sigset_t* sigset)
{
    ntt_loop_worker_ctx_t* context = ctx;
    ntt_loop_t* self               = context->loop;

    struct epoll_event event;
    int rc = 0;

    for (;;) {
        ntt_loop_drain_combine(self);

        rc = epoll_pwait(
            self->epoll_fd,
            &event,
            1,
            -1,
            sigset);

        if ntt_unlikely (rc <= 0) {
            break;
        }

        ntt_epoll_event_ready(&event);
    }
}

void ntt_loop_follower_leave(void* ctx)
{
    // TODO(vg): ...
}

const static ntt_worker_cbs_t g_ntt_loop_follower_cbs = {
    .enter_cb = ntt_loop_follower_enter,
    .svc_cb   = ntt_loop_svc,
    .leave_cb = ntt_loop_follower_leave,
};

void ntt_loop_start_task_svc(void* payload)
{
    ntt_loop_t* self = *(ntt_loop_t**)payload;

    self->cbs.on_start(self->ctx, ntt_queue_self());
}

void ntt_loop_leader_enter(void* ctx, ntt_worker_t* worker)
{
    ntt_loop_worker_ctx_t* context = ctx;

    context->worker = ntt_worker_acquire(worker);

    ntt_loop_t* self = context->loop;

    pthread_mutex_lock(&self->mtx);
    self->enter_cnt += 1;
    while (self->enter_cnt < self->width) {
        pthread_cond_wait(&self->cv, &self->mtx);
    }
    pthread_mutex_unlock(&self->mtx);

    t_loop = self;

    ntt_queue_t* queue = ntt_malloc(sizeof(ntt_queue_t));

    ntt_queue_init(
        queue,
        &self->combine,
        (ntt_queue_wakeup_cb_t*)ntt_combine_push,
        self);

    ntt_loop_work_leave(self);

    void* task    = ntt_task_create(ntt_loop_start_task_svc);
    *(void**)task = self;

    ntt_queue_push_task(queue, task);
    ntt_queue_release(queue);
}

const static ntt_worker_cbs_t g_ntt_loop_leader_cbs = {
    .enter_cb = ntt_loop_leader_enter,
    .svc_cb   = ntt_loop_svc,
    .leave_cb = ntt_loop_follower_leave,
};

static void* ntt_loop_worker_svc(void* ctx)
{
    ntt_worker_svc(g_ntt_loop_follower_cbs, ctx);
    return NULL;
}

int ntt_loop_run(ntt_loop_cbs_t cbs, void* ctx, unsigned width)
{
    if (width == 0) {
        errno = EINVAL;
        return -1;
    }

    ntt_loop_t* self = ntt_malloc(sizeof(ntt_loop_t) + sizeof(ntt_loop_worker_ctx_t) * width);

    if (self == NULL) {
        errno = ENOMEM;
        return -1;
    }

    int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd == -1) {
        return -1;
    }

    self->cbs      = cbs;
    self->ctx      = ctx;
    self->epoll_fd = epoll_fd;
    pthread_mutex_init(&self->mtx, NULL);
    pthread_cond_init(&self->cv, NULL);
    self->enter_cnt = 0;
    self->width     = width;
    self->work_cnt  = 1;

    ntt_combine_init(&self->combine);

    unsigned i = 0;

    self->workers[i].thread_id = pthread_self();
    self->workers[i].worker    = NULL;
    self->workers[i].loop      = self;
    ++i;

    for (; i < width; ++i) {
        self->workers[i].loop   = self;
        self->workers[i].worker = NULL;

        int rc = pthread_create(
            &self->workers[i].thread_id,
            NULL,
            ntt_loop_worker_svc,
            &self->workers[i]);

        if (rc != 0) {
            // TODO(vg): handle thread creating error
            abort();
        }
    }

    ntt_worker_svc(g_ntt_loop_leader_cbs, &self->workers[0]);

    for (i = 1; i < width; ++i) {
        pthread_join(self->workers[i].thread_id, NULL);
    }

    ntt_loop_destroy(self);
    ntt_free(self);

    return 0;
}

void ntt_loop_destroy(ntt_loop_t* self)
{
    // TODO(vg): ...
}

void ntt_loop_work_enter(ntt_loop_t* self)
{
    size_t prev = atomic_fetch_add(&self->work_cnt, 1);
    assert(prev > 0 && "[ntt_loop] work enter on already destroyed loop");
}

void ntt_loop_work_leave(ntt_loop_t* self)
{
    size_t prev = atomic_fetch_sub(&self->work_cnt, 1);
    assert(prev > 0 && "[ntt_loop] work leave on already destroyed loop");

    if (prev == 1) {
        unsigned i = 0;
        for (; i < self->width; ++i) {
            ntt_worker_release(self->workers[i].worker);
        }
    }
}

// static void ntt_loop_leader_enter(void* ctx, ntt_worker_t* worker)
// {
//     ntt_loop_t* self = ctx;

//     self->workers[self->width - 1].worker = ntt_worker_acquire(worker);

//     pthread_mutex_lock(&self->mtx);
//     while (self->followers_ready < self->width - 1) {
//         pthread_cond_wait(&self->cnd, &self->mtx);
//     }
//     pthread_mutex_unlock(&self->mtx);

//     self->work_cnt = 1;
//     self->cbs.on_start(self, self->ctx);
//     ntt_loop_work_leave(self);
// }

// static void ntt_loop_follower_enter(void* ctx, ntt_worker_t* worker)
// {
//     ntt_loop_t* self = ctx;

//     pthread_mutex_lock(&self->mtx);
//     while (!self->followers_created) {
//         pthread_cond_wait(&self->cnd, &self->mtx);
//     }

//     unsigned i = 0;
//     for (; i < self->width - 1; ++i) {
//         if (pthread_equal(pthread_self(), self->workers[i].thread_id)) {
//             self->workers[i].worker = ntt_worker_acquire(worker);
//         }
//     }
//     self->followers_ready += 1;
//     pthread_cond_signal(&self->cnd);
//     pthread_mutex_unlock(&self->mtx);
// }

// static void ntt_loop_worker_leave(void* ctx)
// {
//     // TODO: ...
// }

// static void ntt_loop_worker_svc(void* ctx, ntt_sigset_t* sigset)
// {
//     ntt_loop_t* self = ctx;

//     struct epoll_event event;

//     int rc = 0;

// repeat:
//     rc = epoll_pwait(
//         self->epoll_fd,
//         &event,
//         1,
//         -1,
//         (sigset_t*)sigset);

//     if ntt_likely (rc <= 0) {
//         return;
//     }

//     ntt_epoll_event_ready(&event);
//     goto repeat;
// }

// static void ntt_loop_leader_worker_svc(void* ctx, ntt_sigset_t* sigset)
// {
//     ntt_loop_t* self = ctx;

//     struct epoll_event event;

//     int rc = 0;

// repeat:
//     rc = epoll_pwait(
//         self->epoll_fd,
//         &event,
//         1,
//         -1,
//         (sigset_t*)sigset);

//     // if ntt_likely (rc <= 0) {
//     //     if (self->got_sighup != 0) {
//     //         self->got_sighup = 0;
//     //         self->cbs.on_signal(self, SIGHUP);
//     //     }
//     //     if (self->got_sigint != 0) {
//     //         self->got_sigint = 0;
//     //         self->cbs.on_signal(self, SIGINT);
//     //     }
//     //     if (self->got_sigterm != 0) {
//     //         self->got_sigterm = 0;
//     //         self->cbs.on_signal(self, SIGTERM);
//     //     }
//     //     return;
//     // }

//     ntt_epoll_event_ready(&event);
//     goto repeat;
// }

// const static ntt_worker_cbs_t g_ntt_loop_follower_cbs = {
//     .enter_cb = ntt_loop_follower_enter,
//     .leave_cb = ntt_loop_worker_leave,
//     .svc_cb   = ntt_loop_worker_svc,
// };

// const static ntt_worker_cbs_t g_ntt_loop_leader_cbs = {
//     .enter_cb = ntt_loop_leader_enter,
//     .leave_cb = ntt_loop_worker_leave,
//     .svc_cb   = ntt_loop_leader_worker_svc,
// };

// static void* ntt_loop_worker_svc(void* ctx)
// {
//     ntt_worker_svc(g_ntt_loop_follower_cbs, ctx);
//     return NULL;
// }

// static int ntt_epoll_create1_or_abort(int flags)
// {
//     int fd = epoll_create1(flags);
//     if (fd == -1) {
//         abort();
//     }
//     return fd;
// }

// static ntt_loop_t* g_main_loop = NULL;

// void ntt_handle_signal(int sig)
// {
//     switch (sig) {
//     case SIGTERM:
//         g_main_loop->got_sigterm = 1;
//         break;
//     case SIGINT:
//         g_main_loop->got_sigint = 1;
//         break;
//     case SIGHUP:
//         g_main_loop->got_sighup = 1;
//         break;
//     default:
//         break;
//     }
// }

// int ntt_loop_run(ntt_loop_cbs_t cbs, void* ctx, unsigned width)
// {
//     if (width == 0) {
//         return -1;
//     }

//     // TODO(vgusev): limit width
//     // TODO(vgusev): place ntt_loop_t and related on stack
//     ntt_loop_t* self = malloc(sizeof(ntt_loop_t) + sizeof(ntt_loop_thread_ctx_t) * width);

//     if (self == NULL) {
//         return -1;
//     }

//     self->cbs      = cbs;
//     self->ctx      = ctx;
//     self->epoll_fd = ntt_epoll_create1_or_abort(EPOLL_CLOEXEC);
//     pthread_mutex_init(&self->mtx, NULL);
//     self->width             = width;
//     self->followers_created = 0;
//     self->followers_ready   = 0;
//     self->got_sigint        = 0;
//     self->got_sighup        = 0;
//     self->got_sigterm       = 0;
//     pthread_cond_init(&self->cnd, NULL);
//     self->work_cnt = 0;

//     sigset_t block;
//     sigemptyset(&block);
//     sigaddset(&block, SIGINT);
//     sigaddset(&block, SIGTERM);
//     sigaddset(&block, SIGHUP);

//     ntt_sigset_t origin_mask;
//     sigemptyset((sigset_t*)origin_mask.data);

//     // Block signals and save origin_mask
//     pthread_sigmask(SIG_BLOCK, &block, (sigset_t*)origin_mask.data);

//     g_main_loop = self;

//     struct sigaction sa = { 0 };
//     sigemptyset(&sa.sa_mask);
//     sa.sa_flags   = SA_RESTART;
//     sa.sa_handler = ntt_handle_signal;

//     sigaction(SIGINT, &sa, NULL);
//     sigaction(SIGTERM, &sa, NULL);
//     sigaction(SIGHUP, &sa, NULL);

//     pthread_mutex_lock(&self->mtx);
//     unsigned i = 0;
//     for (i = 0; i < width - 1; ++i) {
//         pthread_create(&self->workers[i].thread_id, NULL, ntt_loop_worker_svc, self);
//     }

//     self->workers[width - 1].thread_id = pthread_self();
//     self->followers_created            = 1;
//     pthread_cond_broadcast(&self->cnd);
//     pthread_mutex_unlock(&self->mtx);

//     ntt_worker_svc_with_mask(g_ntt_loop_leader_cbs, origin_mask, self);

//     for (i = 0; i < width; ++i) {
//         if (!pthread_equal(pthread_self(), self->workers[i].thread_id)) {
//             pthread_join(self->workers[i].thread_id, NULL);
//         }
//     }

//     ntt_loop_destroy(self);
//     free(self);

//     return 0;
// }

// void ntt_loop_destroy(ntt_loop_t* self)
// {
//     // self->vptr = NULL;
//     // self->ctx = NULL;
//     close(self->epoll_fd);
//     pthread_mutex_destroy(&self->mtx);
//     // self->width = 0;
//     // self->followers_created = 0;
//     // self->followers_ready = 0;
//     pthread_cond_destroy(&self->cnd);
//     // self->active = 0;
// }

// ntt_loop_t* ntt_loop_acquire(ntt_loop_t* self)
// {
//     if (self != NULL) {
//         ntt_loop_work_enter(self);
//     }
//     return self;
// }

// void ntt_loop_release(ntt_loop_t* self)
// {
//     if (self != NULL) {
//         ntt_loop_work_leave(self);
//     }
// }

// void ntt_loop_work_enter(ntt_loop_t* self)
// {
//     size_t prev = atomic_fetch_add(&self->work_cnt, 1);
//     assert(prev > 0);
// }

// void ntt_loop_work_leave(ntt_loop_t* self)
// {
//     size_t prev = atomic_fetch_sub(&self->work_cnt, 1);
//     assert(prev > 0);
//     if (prev == 1) {
//         unsigned i = 0;
//         for (; i < self->width; ++i) {
//             ntt_worker_release(self->workers[i].worker);
//         }
//     }
// }

// typedef struct ntt_barrier_task {
//     atomic_size_t refs;
//     ntt_task_t* task;
//     ntt_task_node_t task_nodes[];
// } ntt_barrier_task_t;

// static void ntt_loop_barrier_task_svc(void* ctx)
// {
//     ntt_task_node_t* task_node = ctx;

//     ntt_barrier_task_t* self = *(ntt_barrier_task_t**)task_node->payload;

//     if (atomic_fetch_sub(&self->refs, 1) != 1) {
//         return;
//     }

//     ntt_task_svc_inl(self->task);
//     ntt_task_destroy(self->task);
//     free(self);
// }

// static void ntt_loop_barrier_dummy_call(void* ctx)
// {
// }

// static void ntt_loop_post_barrier_task(ntt_loop_t* self, ntt_task_t* task)
// {
//     struct ntt_barrier_task* barrier = malloc(sizeof(struct ntt_barrier_task) + sizeof(ntt_task_node_t) * self->width);
//     barrier->task                    = task;
//     barrier->refs                    = self->width;
//     unsigned i                       = 0;
//     for (; i < self->width; ++i) {
//         ntt_task_t* nth_task                 = ntt_task_init(&barrier->task_nodes[i], ntt_loop_barrier_dummy_call, ntt_loop_barrier_task_svc);
//         *(struct ntt_barrier_task**)nth_task = barrier;
//         ntt_worker_push_task(self->workers[i].worker, nth_task);
//     }
// }

// static void ntt_loop_epoll_event_canceled(void* payload)
// {
//     ntt_epoll_event_t* self = *(ntt_epoll_event_t**)(payload);
//     ntt_loop_t* loop        = self->loop;
//     ntt_epoll_event_cancelled(self);
//     ntt_loop_work_leave(loop);
// }

// int ntt_loop_add_epoll_event(ntt_loop_t* self, ntt_epoll_event_t* epoll_event)
// {
//     struct epoll_event event;
//     event.data.ptr    = epoll_event;
//     event.events      = epoll_event->events;
//     epoll_event->loop = self;
//     int rc            = epoll_ctl(self->epoll_fd, EPOLL_CTL_ADD, epoll_event->fd, &event);
//     if (rc != 0) {
//         return errno;
//     }
//     ntt_loop_work_enter(self);
//     return 0;
// }

// int ntt_loop_del_epoll_event(ntt_loop_t* self, ntt_epoll_event_t* epoll_event)
// {
//     abort();
//     // int rc = epoll_ctl(self->epoll_fd, EPOLL_CTL_DEL, epoll_event->fd, NULL);
//     // if (rc == -1) {
//     //     return errno;
//     // }
//     // ntt_task_t* task           = ntt_task_create(ntt_loop_epoll_event_canceled, NULL);
//     // *(ntt_epoll_event_t**)task = epoll_event;
//     // ntt_loop_post_barrier_task(self, task);
//     // return 0;
// }
