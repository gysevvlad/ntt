#include "./queue_heap.h"

#include "./malloc.h"
#include "./queue.h"

#include <assert.h>

static inline uint32_t* ntt_queue_heap_pos_of(ntt_queue_t* queue)
{
    return &queue->timer_pos;
}

static inline int ntt_queue_heap_less(
    const ntt_queue_heap_elem_t* lhs,
    const ntt_queue_heap_elem_t* rhs)
{
    return lhs->deadline < rhs->deadline ? 1 : 0;
}

/**
 * @brief Write an entry at the given position and fix up its back reference.
 */
static inline void ntt_queue_heap_place(
    ntt_queue_heap_t* self,
    uint32_t pos,
    ntt_queue_heap_elem_t elem)
{
    self->elems[pos]                       = elem;
    *ntt_queue_heap_pos_of(elem.queue)     = pos;
}

static void ntt_queue_heap_sift_up(
    ntt_queue_heap_t* self,
    uint32_t pos,
    ntt_queue_heap_elem_t elem)
{
    while (pos > 0) {
        uint32_t parent = (pos - 1) / 2;

        if (!ntt_queue_heap_less(&elem, &self->elems[parent])) {
            break;
        }

        ntt_queue_heap_place(self, pos, self->elems[parent]);
        pos = parent;
    }

    ntt_queue_heap_place(self, pos, elem);
}

static void ntt_queue_heap_sift_down(
    ntt_queue_heap_t* self,
    uint32_t pos,
    ntt_queue_heap_elem_t elem)
{
    for (;;) {
        uint32_t left = (pos * 2) + 1;

        if (left >= self->size) {
            break;
        }

        uint32_t child = left;
        uint32_t right = left + 1;

        if (right < self->size
            && ntt_queue_heap_less(&self->elems[right], &self->elems[left])) {
            child = right;
        }

        if (!ntt_queue_heap_less(&self->elems[child], &elem)) {
            break;
        }

        ntt_queue_heap_place(self, pos, self->elems[child]);
        pos = child;
    }

    ntt_queue_heap_place(self, pos, elem);
}

static void ntt_queue_heap_reserve(ntt_queue_heap_t* self, uint32_t cnt)
{
    if (cnt <= self->capacity) {
        return;
    }

    uint32_t capacity = self->capacity == 0
        ? NTT_QUEUE_HEAP_MIN_CAPACITY
        : self->capacity;

    while (capacity < cnt) {
        capacity *= 2;
    }

    self->elems = ntt_realloc_or_die(
        self->elems,
        sizeof(ntt_queue_heap_elem_t) * capacity);
    self->capacity = capacity;
}

void ntt_queue_heap_init(ntt_queue_heap_t* self)
{
    self->elems    = NULL;
    self->size     = 0;
    self->capacity = 0;
}

void ntt_queue_heap_deinit(ntt_queue_heap_t* self)
{
    assert(self->size == 0 && "[ntt_queue_heap] queues are still registered");

    ntt_free(self->elems);
    ntt_queue_heap_init(self);
}

void ntt_queue_heap_insert(
    ntt_queue_heap_t* self,
    ntt_queue_t* queue,
    uint64_t deadline)
{
    assert(*ntt_queue_heap_pos_of(queue) == NTT_QUEUE_HEAP_NONE);

    ntt_queue_heap_reserve(self, self->size + 1);

    uint32_t pos = self->size;
    self->size   = pos + 1;

    ntt_queue_heap_elem_t elem;
    elem.deadline = deadline;
    elem.queue    = queue;

    ntt_queue_heap_sift_up(self, pos, elem);
}

void ntt_queue_heap_decrease(
    ntt_queue_heap_t* self,
    ntt_queue_t* queue,
    uint64_t deadline)
{
    uint32_t pos = *ntt_queue_heap_pos_of(queue);

    assert(pos < self->size);
    assert(deadline <= self->elems[pos].deadline);

    ntt_queue_heap_elem_t elem = self->elems[pos];
    elem.deadline              = deadline;

    ntt_queue_heap_sift_up(self, pos, elem);
}

void ntt_queue_heap_remove(ntt_queue_heap_t* self, ntt_queue_t* queue)
{
    uint32_t pos = *ntt_queue_heap_pos_of(queue);

    assert(pos < self->size);

    *ntt_queue_heap_pos_of(queue) = NTT_QUEUE_HEAP_NONE;

    uint32_t last = self->size - 1;
    self->size    = last;

    if (pos == last) {
        return;
    }

    ntt_queue_heap_elem_t elem = self->elems[last];

    if (pos > 0 && ntt_queue_heap_less(&elem, &self->elems[(pos - 1) / 2])) {
        ntt_queue_heap_sift_up(self, pos, elem);
    } else {
        ntt_queue_heap_sift_down(self, pos, elem);
    }
}

ntt_queue_t* ntt_queue_heap_take_expired(ntt_queue_heap_t* self, uint64_t now)
{
    if (self->size == 0 || self->elems[0].deadline > now) {
        return NULL;
    }

    ntt_queue_t* queue = self->elems[0].queue;

    ntt_queue_heap_remove(self, queue);

    return queue;
}
