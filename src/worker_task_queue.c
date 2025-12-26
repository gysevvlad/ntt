#include "ntt/impl/worker_task_queue.h"

#include "ntt/impl/list.h"
#include "ntt/impl/task.h"
#include "ntt/impl/worker.h"
#include "ntt/worker.h"

#include <pthread.h>

typedef struct ntt_worker_task_queue_wakeup_task {
    ntt_worker_task_queue_t* task_queue;
    int state;
} ntt_worker_task_queue_wakeup_task_t;

static void ntt_worker_task_queue_wakeup_task_svc(ntt_task_t* task)
{
    ntt_worker_task_queue_wakeup_task_t* self = task;

    if (self->state == 0) {
        self->state = 1;

        ntt_worker_push_task_impl(self->task_queue->worker, task);
    } else {
        self->state = 0;

        self->task_queue->wakeup_task_done_flag = 1;
    }
}

static void ntt_worker_task_queue_wakeup_task_free(ntt_task_t* task)
{
    // do nothing
}

static void ntt_worker_task_queue_wakeup_task_init(
    ntt_task_node_t* task_node,
    ntt_worker_task_queue_t* task_queue)
{
    ntt_worker_task_queue_wakeup_task_t* self = ntt_task_init(
        task_node,
        ntt_worker_task_queue_wakeup_task_svc,
        ntt_worker_task_queue_wakeup_task_free);
    self->task_queue = task_queue;
    self->state      = 0;
}

static void ntt_worker_task_queue_wakeup_task_deinit(
    ntt_worker_task_queue_wakeup_task_t* self)
{
}

void ntt_worker_task_queue_init_impl(
    ntt_worker_task_queue_t* self,
    ntt_worker_t* worker)
{
    self->worker = worker;
    self->up     = 0;
    pthread_spin_init(&self->lock, PTHREAD_PROCESS_PRIVATE);
    ntt_task_list_init(&self->tasks);
    self->requested_flag             = 0;
    self->wakeup_task_requested_flag = 0;
    self->wakeup_task_done_flag      = 0;
    ntt_worker_task_queue_wakeup_task_init(&self->wakeup_task_node, self);
}

void ntt_worker_task_queue_deinit_impl(ntt_worker_task_queue_t* self)
{
    // TODO(vg): ...
}

void ntt_worker_task_queue_svc_impl(
    ntt_worker_task_queue_t* self)
{
    assert(self != NULL);
    assert(self->wakeup_task_done_flag == 0);

    ntt_log("[t:%ld|w:%ld] run task queue\n", ntt_this_thread_key(), ntt_this_worker_key());

    for (;;) {
        ntt_task_list_t tasks;
        ntt_task_list_init(&tasks);
        {
            pthread_spin_lock(&self->lock);
            if (ntt_list_empty(&self->tasks.list)) {
                if (self->wakeup_task_done_flag) {
                    self->wakeup_task_done_flag      = 0;
                    self->wakeup_task_requested_flag = 0;
                }
                self->requested_flag = 0;
                pthread_spin_unlock(&self->lock);
                return;
            }
            ntt_list_swap(&tasks.list, &self->tasks.list);
            pthread_spin_unlock(&self->lock);
        }
        int last = 0;
        do {
            ntt_task_t* task = ntt_task_list_pop(&tasks, &last);
            assert(task != NULL);
            ntt_do_task_inl(task);
            ntt_free_task_inl(task);
        } while (!last);
    }
}

void ntt_worker_task_queue_send_task_impl(
    ntt_worker_task_queue_t* self,
    ntt_task_t* task)
{
    pthread_spin_lock(&self->lock);
    ntt_task_list_push(&self->tasks, task);
    if (!self->requested_flag) {
        self->requested_flag = 1;
        pthread_spin_unlock(&self->lock);
        ntt_worker_wakeup_impl(self->worker);
        return;
    }
    pthread_spin_unlock(&self->lock);
}

void ntt_worker_task_queue_idle_post_impl(
    ntt_worker_task_queue_t* self,
    ntt_worker_task_queue_t* sender_queue,
    ntt_task_t* task)
{
    assert(self != NULL);
    assert(task != NULL);

    int need_idle_wakeup_flag = 0;

    pthread_spin_lock(&self->lock);
    ntt_task_list_push(&self->tasks, task);
    if (!self->requested_flag) {
        /// wakeup not sent
        if (!self->wakeup_task_requested_flag) {
            /// idle wakeup not sent
            self->wakeup_task_requested_flag = 1;
            need_idle_wakeup_flag            = 1;
        }
    }
    pthread_spin_unlock(&self->lock);

    if (need_idle_wakeup_flag) {
        ntt_worker_task_queue_post_impl(sender_queue, &self->wakeup_task_node.payload);
    }
}

void ntt_worker_task_queue_post_impl(
    ntt_worker_task_queue_t* self,
    ntt_task_t* task)
{
    assert(self != NULL);
    assert(task != NULL);

    pthread_spin_lock(&self->lock);
    ntt_task_list_push(&self->tasks, task);
    pthread_spin_unlock(&self->lock);
}

void ntt_worker_task_queue_defer_wakeup_impl(
    ntt_worker_task_queue_t* self,
    ntt_worker_task_queue_t* sender)
{
    assert(self != NULL);
    assert(sender != NULL);

    int need_idle_wakeup_flag = 0;

    pthread_spin_lock(&self->lock);
    if (!self->requested_flag) {
        /// wakeup not sent
        if (!self->wakeup_task_requested_flag) {
            /// idle wakeup not sent
            self->wakeup_task_requested_flag = 1;
            need_idle_wakeup_flag            = 1;
        }
    }
    pthread_spin_unlock(&self->lock);

    if (need_idle_wakeup_flag) {
        ntt_worker_task_queue_post_impl(sender, &self->wakeup_task_node.payload);
    }
}
