#include "./queue.h"

#include "./loop.h"
#include "./task.h"
#include "./task_list.h"
#include "ntt/queue.h"

#include <pthread.h>
#include <threads.h>

static thread_local ntt_queue_t* t_queue = NULL;

ntt_queue_t* ntt_queue_self()
{
    return t_queue;
}

int ntt_queue_unlinked(ntt_queue_t* queue)
{
    return ntt_node_unlinked(&queue->node);
}

int ntt_queue_empty(ntt_queue_t* queue)
{
    return ntt_task_list_empty(&queue->task_list);
}

ntt_task_t* ntt_queue_front(ntt_queue_t* queue)
{
    assert(ntt_queue_empty(queue) == 0);

    // Assumes the push occurs beforehand with release memory ordering.
    // The sentinel.next node is no longer modified.
    // Therefore, it is safe to read the head pointer without acquiring a lock.

    return ntt_task_list_front(&queue->task_list);
}

ntt_task_t* ntt_queue_next(ntt_queue_t* queue)
{
    assert(ntt_queue_unlinked(queue) == 1);
    assert(ntt_queue_empty(queue) == 0);

    // TODO(vg): should I move lock to ntt_task_list_next_impl?

    pthread_spin_lock(&queue->lock);
    ntt_task_t* task = ntt_task_list_next(&queue->task_list);
    pthread_spin_unlock(&queue->lock);

    return task;
}

void ntt_queue_push_task(ntt_queue_t* queue, ntt_task_t* task)
{
    assert(ntt_task_unlinked(task) == 1);

    pthread_spin_lock(&queue->lock);
    int was_empty = ntt_task_list_push(&queue->task_list, task);
    pthread_spin_unlock(&queue->lock);

    if (was_empty) {
        ntt_queue_acquire(queue);
        queue->wakeup_cb(queue->context, queue);
    }
}

int ntt_queue_push_task_without_wakeup(ntt_queue_t* queue, ntt_task_t* task)
{
    assert(ntt_task_unlinked(task) == 1);

    pthread_spin_lock(&queue->lock);
    int was_empty = ntt_task_list_push(&queue->task_list, task);
    pthread_spin_unlock(&queue->lock);

    return was_empty;
}

size_t ntt_queue_svc_with_limit(
    ntt_queue_t* queue,
    size_t limit)
{
    assert(queue != NULL);
    assert(limit > 0);
    assert(ntt_queue_empty(queue) == 0);
    assert(ntt_queue_unlinked(queue) == 1);

    assert(t_queue == NULL);
    t_queue = queue;

    size_t cnt       = 0;
    ntt_task_t* prev = NULL;
    ntt_task_t* task = ntt_queue_front(queue);
    do {
        ntt_task_svc_inl(task);
        prev = task;
        task = ntt_queue_next(queue);
        ntt_task_destroy_inl(prev);
        cnt += 1;
        if (ntt_unlikely(cnt == limit)) {
            if (ntt_unlikely(task != NULL)) {
                cnt = 0;
            }
            break;
        }
    } while (task != NULL);

    assert(t_queue == queue);
    t_queue = NULL;

    if (cnt != 0) {
        ntt_queue_release(queue);
    }

    return cnt;
}

void ntt_queue_svc(
    ntt_queue_t* self)
{
    assert(ntt_queue_empty(self) == 0);
    assert(ntt_queue_unlinked(self) == 1);

    assert(t_queue == NULL);
    t_queue = self;

    ntt_task_t* prev = NULL;
    ntt_task_t* task = ntt_queue_front(self);
    do {
        ntt_task_svc_inl(task);
        prev = task;
        task = ntt_queue_next(self);
        ntt_task_destroy_inl(prev);
    } while (task != NULL);

    assert(t_queue == self);
    t_queue = NULL;

    ntt_queue_release(self);
}

void ntt_queue_init(
    ntt_queue_t* self,
    void* context,
    ntt_queue_wakeup_cb_t* wakeup_cb,
    ntt_loop_t* loop)
{
    self->external_refs = 1;
    ntt_task_list_init(&self->task_list);
    pthread_spin_init(&self->lock, 0);
    ntt_node_init(&self->node);
    self->context   = context;
    self->wakeup_cb = wakeup_cb;
    self->loop      = loop;

    if (self->loop != NULL) {
        ntt_loop_work_enter(self->loop);
    }
}

void ntt_queue_deinit(
    ntt_queue_t* self)
{
    assert(ntt_queue_use_count(self) == 0);
    assert(ntt_queue_empty(self) == 1);
    assert(ntt_queue_unlinked(self) == 1);

    if (self->loop != NULL) {
        ntt_loop_work_leave(self->loop);
    }

    pthread_spin_destroy(&self->lock);
}

ntt_queue_t* ntt_queue_create()
{
    // TODO(vg): impl
    return NULL;
}

void ntt_queue_push(ntt_queue_t* queue, ntt_task_t* task)
{
    ntt_queue_push_task(queue, task);
}

ntt_queue_t* ntt_queue_acquire(
    ntt_queue_t* self)
{
    assert(ntt_queue_use_count(self) != 0);

    atomic_fetch_add(&self->external_refs, 1);

    return self;
}

void ntt_queue_release(
    ntt_queue_t* self)
{
    size_t prev = atomic_fetch_sub(&self->external_refs, 1);
    if (prev == 1) {
        ntt_queue_deinit(self);
        ntt_free(self);
    }
}
