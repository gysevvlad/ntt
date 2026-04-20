#include "./task.h"
#include "ntt/log.h"

#include "./task_list.h"
#include "./worker.h"
#include "ntt/impl/atomic.h"
#include "queue.h"

#include <assert.h>
#include <bits/types/sigset_t.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <threads.h>

#define SIGRTNTTUP SIGRTMIN

static thread_local ntt_worker_t* t_worker_self = NULL;

static atomic_size_t g_worker_next_key = 1;
static atomic_size_t g_thread_next_key = 1;

static thread_local size_t g_thread_key = 0;

size_t ntt_this_thread_key()
{
    if (g_thread_key == 0) {
        g_thread_key = atomic_fetch_add(&g_thread_next_key, 1);
    }
    return g_thread_key;
}

size_t ntt_this_worker_key()
{
    if (t_worker_self != NULL) {
        return t_worker_self->key;
    }
    return 0;
}

static void ntt_process_nttup_signal_handler(int signum, siginfo_t* info, void* context)
{
    (void)signum;
    (void)context;

    ntt_worker_t* self = info->si_value.sival_ptr;

    self->need_drain_local_queue = 1;
}

static void ntt_process_setup_signal_action()
{
    struct sigaction action = { 0 };
    action.sa_flags         = SA_SIGINFO;
    action.sa_sigaction     = ntt_process_nttup_signal_handler;
    int rc                  = sigaction(SIGRTNTTUP, &action, NULL);
}

static void ntt_worker_stop_task_svc(ntt_task_t* task)
{
    ntt_worker_t* self = *(ntt_worker_t**)task;
    self->stopped      = 1;
}

static void ntt_worker_stop_task_free(ntt_task_t* task)
{
    // do nothing
}

static void ntt_worker_local_queue_wakeup(void* context, ntt_queue_t* queue)
{
    (void)queue;

    assert(context != NULL);

    ntt_worker_t* self = context;

    ntt_log("[t:%ld|w:%ld] send wakeup to w:%ld\n",
        ntt_this_thread_key(),
        ntt_this_worker_key(),
        self->key);

    union sigval sigval;
    sigval.sival_ptr = self;
    int rc           = 0;
    do {
        rc = pthread_sigqueue(self->id, SIGRTNTTUP, sigval);
    } while (rc == EAGAIN);
    assert(rc == 0);
}

void ntt_worker_init(
    ntt_worker_t* self,
    ntt_worker_cbs_t cbs,
    void* ctx)
{
    self->refs = 1;
    self->cbs  = cbs;
    self->ctx  = ctx;
    self->id   = pthread_self();
    self->key  = atomic_fetch_add(&g_worker_next_key, 1);
    ntt_queue_init(&self->local_queue, self, ntt_worker_local_queue_wakeup, NULL);
    self->need_drain_local_queue = 0;

    ntt_task_t* task = ntt_task_init(
        &self->stop_task_node,
        ntt_worker_stop_task_svc,
        ntt_worker_stop_task_free);
    *(ntt_worker_t**)task = self;

    self->stopped = 0;
}

void ntt_worker_deinit(
    ntt_worker_t* self)
{
}

ntt_worker_t* ntt_worker_acquire(
    ntt_worker_t* self)
{
    if (self != NULL) {
        size_t prev = atomic_fetch_add(&self->refs, 1);
        assert(prev != 0 && "[ntt_worker_acquire]: trying to acquire already released object");
    }
    return self;
}

void ntt_worker_release(
    ntt_worker_t* self)
{
    if (self != NULL) {
        size_t prev = atomic_fetch_sub(&self->refs, 1);
        assert(prev != -1 && "[ntt_worker_release]: trying to release already released object");
        if (prev == 1) {
            ntt_worker_push_task(self, &self->stop_task_node.payload);
        }
    }
}

NTT_EXPORT int ntt_worker_svc_with_mask(
    ntt_worker_cbs_t cbs,
    sigset_t* origin_mask,
    void* ctx)
{
    ntt_worker_t worker;
    ntt_worker_init(&worker, cbs, ctx);
    ntt_process_setup_signal_action();
    worker.id = pthread_self();

    int result = 0;

    sigset_t nttup;
    sigemptyset(&nttup);
    sigaddset(&nttup, SIGRTNTTUP);

    pthread_sigmask(SIG_BLOCK, &nttup, NULL);

    t_worker_self = &worker;

    worker.cbs.enter_cb(worker.ctx, &worker);
    ntt_worker_release(&worker);

    while (!worker.stopped) {
        ntt_log("[t:%ld|w:%ld] run svc callback\n", ntt_this_thread_key(), ntt_this_worker_key());
        worker.cbs.svc_cb(worker.ctx, origin_mask);
        if (worker.need_drain_local_queue == 1) {
            ntt_queue_svc(&worker.local_queue);
            worker.need_drain_local_queue = 0;
        }
    }
    t_worker_self = NULL;
    worker.cbs.leave_cb(worker.ctx);

    ntt_worker_deinit(&worker);

    pthread_sigmask(SIG_UNBLOCK, &nttup, NULL);

    return result;
}

int ntt_worker_svc(
    ntt_worker_cbs_t cbs,
    void* ctx)
{
    sigset_t origin_mask;
    pthread_sigmask(SIG_SETMASK, NULL, &origin_mask);
    return ntt_worker_svc_with_mask(cbs, &origin_mask, ctx);
}

void ntt_worker_push_task(
    ntt_worker_t* self,
    ntt_task_t* task)
{
    assert(self != NULL);
    assert(task != NULL);

    ntt_log("[t:%ld|w:%ld] push task to w:%ld\n",
        ntt_this_thread_key(),
        ntt_this_worker_key(),
        self->key);

    ntt_queue_push_task(&self->local_queue, task);
}

ntt_worker_t* ntt_worker_self() { return t_worker_self; }
