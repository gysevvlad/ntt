#include "ntt/impl/context.h"
#include "ntt/ec.h"
#include "ntt/impl/thread.h"

#include <pthread.h>
#include <stdlib.h>

void ntt_context_init(ntt_context_t* self)
{
}

ntt_context_t* ntt_context_create()
{
}

void ntt_context_set_diff_width(
    ntt_context_t* self,
    int diff)
{
    pthread_mutex_lock(&self->mtx);

    if (self->started) {
        // ERROR: unsupported
        // TODO(vgusev): send signal to leader thread to add threads
        abort();
    } else {
        if (self->threads_cnt + diff < 0) {
            // ERROR: unmatched +1/-1 width change requests
            abort();
        }
        self->threads_cnt += diff;
    }

    pthread_mutex_unlock(&self->mtx);
}

void ntt_context_follower_stopped_svc(
    ntt_thread_t* thread,
    void* arg)
{
}

ntt_ec_t ntt_context_run(
    ntt_context_t* self)
{
    pthread_mutex_lock(&self->mtx);

    if (self->started) {
        // error: only one thread should be in ntt_context_run
        abort();
    }

    if (atomic_fetch_add(&self->work_cnt, 1) == 0) {
        // no works, returns

        size_t tmp = atomic_fetch_sub(&self->work_cnt, 1);
        if (tmp != 0) {
            // error: found race condition: ntt_context_run / first ntt_context_work_enter
            abort();
        }

        pthread_mutex_unlock(&self->mtx);
        return ntt_make_ok_ec();
    }

    self->leader_thread = pthread_self();

    if (self->threads_cnt > 0) {
        self->threads = malloc(sizeof(ntt_thread_t*) * self->threads_cnt);
        size_t i = 0;
        for (i = 0; i < self->threads_cnt; ++i) {
            self->threads[i] = malloc(sizeof(ntt_thread_t));
            ntt_thread_init(
                self->threads[i],
                ntt_context_follower_stopped_svc,
                self,
                self->epoll_fd);
        }
    }
}

void ntt_context_work_enter(
    ntt_context_t* self)
{
}

void ntt_context_work_leave(
    ntt_context_t* self)
{
}
