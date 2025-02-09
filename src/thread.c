#include "ntt/impl/thread.h"
#include "ntt/impl/event.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <sys/epoll.h>

void ntt_thread_stop_task_svc(ntt_task_t* task)
{
    ntt_thread_t* self = *(ntt_thread_t**)task;
    self->active = 0;
}

void ntt_thread_stop_task_free(ntt_task_t* task)
{
    // do nothing
}

void ntt_thread_drain_tasks(ntt_thread_t* self)
{
    int last;
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

void ntt_thread_up(ntt_thread_t* self) { ntt_thread_drain_tasks(self); }

void ntt_thread_epoll_event(struct epoll_event* ev)
{
    ntt_event_t* event = ev->data.ptr;
    event->cb(event->ctx, ev->events);
}

void ntt_thread_svc(ntt_thread_t* self)
{
    sigset_t sigset;
    sigemptyset(&sigset);
    pthread_sigmask(0, NULL, &sigset);
    sigdelset(&sigset, SIGRTNTTUP);

    struct epoll_event ev;
    while (self->active) {
        int rc = epoll_pwait(self->epoll_fd, &ev, 1, -1, &sigset);
        if (rc == 1) {
            ntt_thread_epoll_event(&ev);
            continue;
        }
        if (rc == -1) {
            if (errno == EINTR) {
                if (self->got_up == 1) {
                    ntt_thread_up(self);
                    self->got_up = 0;
                }
                continue;
            }
            abort(); // shit happens
        }
    }

    self->complete_cb(self, self->context);
}

void* ntt_thread_start_routine(void* context)
{
    ntt_thread_svc(context);
    return NULL;
}

void ntt_up_signal_handler(int signo, siginfo_t* info, void* context)
{
    assert(signo == SIGRTNTTUP);
    ntt_thread_t* thread = info->si_value.sival_ptr;
    thread->got_up = 1;
}

void ntt_thread_setup_signal_action()
{
    struct sigaction action = { 0 };
    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = ntt_up_signal_handler;
    int rc = sigaction(SIGRTNTTUP, &action, NULL);
    assert(rc != -1);
}

void ntt_thread_init(
    ntt_thread_t* self,
    void (*complete_cb)(ntt_thread_t* thread, void* context),
    void* context,
    int epoll_fd)
{
    ntt_thread_setup_signal_action();

    self->epoll_fd = epoll_fd;
    self->active = 1;

    self->complete_cb = complete_cb;
    self->context = context;
    self->stopped = 0;

    pthread_spin_init(&self->tasks_lock, PTHREAD_PROCESS_PRIVATE);
    self->got_up = 0;
    ntt_task_list_init(&self->tasks_lists);

    ntt_task_t* task = ntt_task_init(
        &self->tasks_sentinel,
        ntt_thread_stop_task_svc,
        ntt_thread_stop_task_free);

    *(ntt_thread_t**)task = self;

    // block SIGRTNTTUP
    sigset_t sigset;
    int rc = sigemptyset(&sigset);
    assert(rc != -1);
    rc = sigaddset(&sigset, SIGRTNTTUP);
    assert(rc != -1);
    sigset_t origin_sigset;
    rc = sigprocmask(SIG_BLOCK, &sigset, &origin_sigset);
    assert(rc != -1);

    rc = pthread_create(&self->id, NULL, ntt_thread_start_routine, self);
    assert(rc == 0);

    // return back original sigset
    rc = sigprocmask(SIG_SETMASK, &origin_sigset, NULL);
    assert(rc != -1);
}

void ntt_thread_send_task_impl(ntt_thread_t* self, ntt_task_t* task)
{
    int first;
    pthread_spin_lock(&self->tasks_lock);
    ntt_task_list_push(&self->tasks_lists, task, &first);
    pthread_spin_unlock(&self->tasks_lock);
    if (first) {
        union sigval sigval;
        sigval.sival_ptr = self;
        int rc;
        do {
            rc = pthread_sigqueue(self->id, SIGRTNTTUP, sigval);
        } while (rc == EAGAIN);
        assert(rc == 0);
    }
}

void ntt_thread_send_task(ntt_thread_t* self, ntt_task_t* task)
{
    assert(atomic_load(&self->stopped) == 0 && "ntt thread already stopped");
    ntt_thread_send_task_impl(self, task);
}

void ntt_thread_send_stop(ntt_thread_t* self)
{
    size_t prev = atomic_fetch_add(&self->stopped, 1);
    assert(prev == 0 && "ntt thread already stopped");
    if (prev == 0) {
        ntt_thread_send_task_impl(self, &self->tasks_sentinel.payload);
    }
}

void ntt_thread_destroy(ntt_thread_t* self)
{
    pthread_spin_destroy(&self->tasks_lock);
}
