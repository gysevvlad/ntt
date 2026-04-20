#include "./combine.h"
#include "./list.h"
#include "ntt/defs.h"
#include "ntt/queue.h"
#include "queue.h"

#include <pthread.h>
#include <stddef.h>

void ntt_combine_init(ntt_combine_t* self)
{
    ntt_list_init(&self->list);
    pthread_spin_init(&self->lock, 0);
}

int ntt_combine_empty(ntt_combine_t* self)
{
    return ntt_list_empty(&self->list);
}

void ntt_combine_deinit(ntt_combine_t* self)
{
    assert(ntt_combine_empty(self) == 1);

    pthread_spin_destroy(&self->lock);
}

void ntt_combine_push(ntt_combine_t* self, ntt_queue_t* queue)
{
    assert(ntt_queue_empty(queue) == 0);
    assert(ntt_queue_unlinked(queue) == 1);

    pthread_spin_lock(&self->lock);
    ntt_list_push_back(&self->list, &queue->node);
    pthread_spin_unlock(&self->lock);
}

ntt_queue_t* ntt_combine_try_take(ntt_combine_t* self)
{
    ntt_queue_t* queue = NULL;

    pthread_spin_lock(&self->lock);
    if (ntt_list_empty(&self->list) == 0) {
        queue = ntt_container_of(ntt_list_front(&self->list), ntt_queue_t, node);
        ntt_list_next(&self->list);
    }
    pthread_spin_unlock(&self->lock);

    return queue;
}

int ntt_combine_svc(
    ntt_combine_t* self,
    size_t loop_limit,
    size_t queue_limit)
{
    assert(loop_limit > 0);
    assert(queue_limit > 0);

    size_t cnt         = 0;
    ntt_queue_t* queue = NULL;

    while (cnt < loop_limit) {
        queue = ntt_combine_try_take(self);
        if (queue == NULL) {
            return 0;
        }
        size_t queue_cnt = ntt_queue_svc_with_limit(queue, queue_limit);
        if (queue_cnt == 0) {
            ntt_combine_push(self, queue);
        }
        cnt += 1;
    }

    return 1;
}
