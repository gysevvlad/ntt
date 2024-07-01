#include "ntt/event_queue.h"

#include <assert.h>

void ntt_event_queue_init(struct ntt_event_queue *event_queue) {
  mtx_init(&event_queue->mtx, mtx_plain);
  cnd_init(&event_queue->cnd);
  event_queue->head.next = NULL;
  event_queue->tail = &event_queue->head;
  event_queue->stopped = 0;
}

void ntt_event_queue_push(struct ntt_event_queue *event_queue,
                          struct ntt_event *event) {
  event->next = NULL;
  mtx_lock(&event_queue->mtx);
  event_queue->tail->next = event;
  event_queue->tail = event;
  mtx_unlock(&event_queue->mtx);
  cnd_signal(&event_queue->cnd);
}

void ntt_event_queue_stop(struct ntt_event_queue *event_queue) {
  mtx_lock(&event_queue->mtx);
  assert(task_queue->stopped == 0 &&
         "internal error: task queue already stopped");
  event_queue->stopped = 1;
  cnd_broadcast(&event_queue->cnd);
  mtx_unlock(&event_queue->mtx);
}

struct ntt_event *ntt_event_queue_pop(struct ntt_event_queue *event_queue) {
  mtx_lock(&event_queue->mtx);
  for (;;) {
    if (event_queue->head.next != NULL) {
      struct ntt_event *event = event_queue->head.next;
      event_queue->head.next = event->next;
      if (event == event_queue->tail) {
        event_queue->tail = &event_queue->head;
      }
      mtx_unlock(&event_queue->mtx);
      return event;
    }
    if (event_queue->stopped) {
      mtx_unlock(&event_queue->mtx);
      return NULL;
    }
    cnd_wait(&event_queue->cnd, &event_queue->mtx);
  }
}

void ntt_event_queue_destroy(struct ntt_event_queue *event_queue) {
  cnd_destroy(&event_queue->cnd);
  mtx_destroy(&event_queue->mtx);
}
