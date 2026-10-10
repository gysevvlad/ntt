#include "./queue.h"

#include "./loop.h"
#include "./task.h"
#include "./queue_slab.h"
#include "./timer_service.h"
#include "./task_list.h"
#include "ntt/queue.h"
#include "./combine.h"

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
    pthread_spin_lock(&queue->lock);
    int empty = ntt_task_list_empty(&queue->task_list);
    pthread_spin_unlock(&queue->lock);

    return empty;
}

ntt_task_t* ntt_queue_front(ntt_queue_t* queue)
{
    // Taken under the lock, and not only to read a stable head pointer: this
    // is what pairs with the unlock of the push and makes the payload the
    // producer wrote visible here. Reading the head without it leaves the
    // payload unsynchronized, which is a data race even though the head
    // itself happens to be stable.
    pthread_spin_lock(&queue->lock);

    assert(ntt_task_list_empty(&queue->task_list) == 0);
    ntt_task_t* task = ntt_task_list_front(&queue->task_list);

    pthread_spin_unlock(&queue->lock);

    return task;
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

    ntt_timer_heap_init(&self->timers);
    self->timer_service      = NULL;
    self->timer_reg_gen      = 0;
    self->timer_reg_deadline = UINT64_MAX;
    self->timer_self_ref     = 0;
    self->timer_tombstones   = 0;
    self->slab_index         = NTT_QUEUE_SLAB_NONE;

    // Generations start at one, so this value matches no entry of the timer
    // service. A queue taken from the slab restores the generation of its slot
    // right after this call.
    atomic_store_explicit(&self->timer_gen, 0, memory_order_relaxed);

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

    assert(ntt_timer_heap_empty(&self->timers) == 1);

    ntt_timer_heap_deinit(&self->timers);

    pthread_spin_destroy(&self->lock);
}

/**
 * @brief Body of the task that drains the timers of a queue.
 */
static void ntt_queue_timers_task_svc(void* payload, ntt_task_action_t action)
{
    if (action != NTT_TASK_RUN) {
        return;
    }

    ntt_queue_t* self = *(ntt_queue_t**)payload;

    assert(ntt_queue_self() == self);

    ntt_timer_heap_svc(&self->timers, ntt_now_ns());
    ntt_queue_sync_timers(self);
}

void ntt_queue_wakeup_timers(ntt_queue_t* queue)
{
    ntt_queue_push_task(
        queue,
        ntt_task_create_with_ctx(ntt_queue_timers_task_svc, NULL, queue));
}

void ntt_queue_set_timer_service(ntt_queue_t* queue, ntt_timer_service_t* service)
{
    queue->timer_service = service;
}

void ntt_queue_sync_timers(ntt_queue_t* queue)
{
    assert(ntt_queue_self() == queue);

    // The registration stands only while nobody has bumped the generation
    // since it was made.
    int registered = ntt_queue_timer_generation(queue) == queue->timer_reg_gen;

    uint64_t deadline = 0;

    if (ntt_timer_heap_peek(&queue->timers, &deadline) == 0) {
        if (registered) {
            // Dropping the registration is one atomic add: the entry of the
            // service becomes a tombstone, counted here until the next
            // registration hands the count over.
            ntt_queue_retire_timer_generation(queue);
            queue->timer_tombstones += 1;
        }

        if (queue->timer_self_ref) {
            queue->timer_self_ref = 0;
            ntt_queue_release(queue);
        }

        return;
    }

    // Armed timers keep the queue, and through it the loop, alive. The service
    // holds nothing, so this is the only thing that does.
    if (!queue->timer_self_ref) {
        ntt_queue_acquire(queue);
        queue->timer_self_ref = 1;
    }

    if (registered && queue->timer_reg_deadline <= deadline) {
        return;
    }

    if (registered) {
        // The entry that stands now is replaced by the one below.
        queue->timer_tombstones += 1;
    }

    uint32_t generation = ntt_queue_retire_timer_generation(queue);

    queue->timer_reg_gen      = generation;
    queue->timer_reg_deadline = deadline;

    uint32_t tombstones     = queue->timer_tombstones;
    queue->timer_tombstones = 0;

    ntt_timer_service_register(
        queue->timer_service,
        queue->slab_index,
        generation,
        deadline,
        tombstones);
}

ntt_queue_t* ntt_queue_create()
{
    ntt_loop_t* loop = ntt_loop_self();

    assert(loop != NULL && "[ntt_queue_create] not in a loop context");

    return ntt_queue_create_in(loop);
}

ntt_queue_t* ntt_queue_create_in(ntt_loop_t* loop)
{
    uint32_t index = NTT_QUEUE_SLAB_NONE;

    ntt_queue_t* self = ntt_queue_slab_take(ntt_loop_queue_slab(loop), &index);

    // The slab owns the generation of the slot across reuse, so it has to
    // survive the init that follows.
    uint32_t generation = ntt_queue_timer_generation(self);

    ntt_queue_init(
        self,
        ntt_loop_combine(loop),
        (ntt_queue_wakeup_cb_t*)ntt_combine_push,
        loop);

    atomic_store_explicit(&self->timer_gen, generation, memory_order_relaxed);

    self->slab_index    = index;
    self->timer_service = ntt_loop_timer_service(loop);

    return self;
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
        ntt_loop_t* loop = self->loop;
        uint32_t index   = self->slab_index;

        ntt_queue_deinit(self);

        if (index != NTT_QUEUE_SLAB_NONE) {
            ntt_queue_slab_give(ntt_loop_queue_slab(loop), index);
        } else {
            ntt_free(self);
        }

        // Last of all: this is the work of the loop that the queue held, and
        // the moment it is gone the loop may finish and tear down the slab
        // this queue was just given back to.
        if (loop != NULL) {
            ntt_loop_work_leave(loop);
        }
    }
}

ntt_loop_t* ntt_queue_loop(ntt_queue_t* self)
{
    return self->loop;
}
