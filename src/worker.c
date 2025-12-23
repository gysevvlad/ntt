#include "ntt/worker.h"

#include "ntt/sigset.h"

#include "ntt/impl/atomic.h"
#include "ntt/impl/task_list.h"
#include "ntt/impl/worker.h"

#include <assert.h>
#include <bits/types/sigset_t.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <threads.h>

#define SIGRTNTTUP SIGRTMIN

static thread_local ntt_worker_t* t_worker_self = NULL;

static void ntt_process_nttup_signal_handler(int signum, siginfo_t* info, void* context)
{
    (void)signum;
    (void)context;

    ntt_worker_t* worker = info->si_value.sival_ptr;
    worker->tasks_up     = 1;
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

static void ntt_worker_drain_tasks(ntt_worker_t* self)
{
    int last = 0;
    do {
        ntt_task_t* task = ntt_task_list_front(&self->tasks_lists);
        assert(task != NULL);
        ntt_do_task_inl(task);
        pthread_spin_lock(&self->tasks_lock);
        ntt_task_list_pop(&self->tasks_lists, &last);
        pthread_spin_unlock(&self->tasks_lock);
        ntt_free_task_inl(task);
    } while (!last);
}

void ntt_worker_init(
    ntt_worker_t* self,
    ntt_worker_cbs_t cbs,
    void* ctx)
{
    self->refs = 1;

    self->cbs = cbs;
    self->ctx = ctx;

    self->tasks_up = 0;
    pthread_spin_init(&self->tasks_lock, 0);
    ntt_task_list_init(&self->tasks_lists);
    self->tasks_pending = 0;
    // self->wakeup_gen = 0;
    // self->task_gen = 0;

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
    pthread_spin_destroy(&self->tasks_lock);
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
            ntt_worker_send_task(self, &self->stop_task_node.payload);
        }
    }
}

NTT_EXPORT int ntt_worker_svc_with_mask(
    ntt_worker_cbs_t cbs,
    ntt_sigset_t origin_mask,
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

    // int signal = 1;
    // for (; signal < NSIG; ++signal) {
    //     if (sigismember((sigset_t*)signal_mask.data, signal)) {
    //         printf("Signal %i is blocked\n", signal);
    //     }
    // }

    worker.cbs.enter_cb(worker.ctx, &worker);
    ntt_worker_release(&worker);

    while (!worker.stopped) {
        worker.cbs.svc_cb(worker.ctx, &origin_mask);
        if (worker.tasks_up) {
            ntt_worker_drain_tasks(&worker);
            worker.tasks_up = 0;
        }
    }

    worker.cbs.leave_cb(worker.ctx);

    ntt_worker_deinit(&worker);

    pthread_sigmask(SIG_UNBLOCK, &nttup, NULL);

    return result;
}

int ntt_worker_svc(
    ntt_worker_cbs_t cbs,
    void* ctx)
{
    ntt_sigset_t origin_mask;
    pthread_sigmask(SIG_SETMASK, NULL, (sigset_t*)origin_mask.data);
    return ntt_worker_svc_with_mask(cbs, origin_mask, ctx);
}

void ntt_worker_send_task(
    ntt_worker_t* self,
    ntt_task_t* task)
{
    int first   = 0;
    int pending = 0;
    pthread_spin_lock(&self->tasks_lock);
    ntt_task_list_push(&self->tasks_lists, task, &first);
    if (self->tasks_pending != 0) {
        self->tasks_pending = 0;
        pending             = 1;
    }
    pthread_spin_unlock(&self->tasks_lock);
    if (first || pending) {
        union sigval sigval;
        sigval.sival_ptr = self;
        int rc           = 0;
        do {
            rc = pthread_sigqueue(self->id, SIGRTNTTUP, sigval);
        } while (rc == EAGAIN);
        assert(rc == 0);
    }
}

void ntt_worker_post_task(
    ntt_worker_t* self,
    ntt_task_t* task)
{
    assert(self);

    int first   = 0;
    int pending = 0;
    pthread_spin_lock(&self->tasks_lock);
    ntt_task_list_push(&self->tasks_lists, task, &first);
    if (first != 0) {
        self->tasks_pending = 1;
    }
    pthread_spin_unlock(&self->tasks_lock);
}

void ntt_worker_wakeup(
    ntt_worker_t* self)
{
    assert(self);

    union sigval sigval;
    sigval.sival_ptr = self;
    int rc           = 0;
    do {
        rc = pthread_sigqueue(self->id, SIGRTNTTUP, sigval);
    } while (rc == EAGAIN);
    assert(rc == 0);
}

ntt_worker_t* ntt_worker_self() { return t_worker_self; }
