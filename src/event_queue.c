#include "ntt/event_queue.h"
#include "ntt/malloc.h"
#include <assert.h>
#include <threads.h>

void ntt_event_queue_init(struct ntt_event_queue *event_queue) {
  mtx_init(&event_queue->m, mtx_plain);
  cnd_init(&event_queue->c);
  event_queue->head.next = NULL;
  event_queue->tail = &event_queue->head;
  event_queue->stopped = 0;
}

void ntt_event_queue_push(struct ntt_event_queue *event_queue,
                          struct ntt_task_node *task_node) {
  task_node->next = NULL;
  mtx_lock(&event_queue->m);
  event_queue->tail->next = task_node;
  event_queue->tail = task_node;
  mtx_unlock(&event_queue->m);
  cnd_signal(&event_queue->c);
}

void ntt_event_queue_stop(struct ntt_event_queue *event_queue) {
  mtx_lock(&event_queue->m);
  assert(task_queue->stopped == 0 &&
         "internal error: task queue already stopped");
  event_queue->stopped = 1;
  cnd_broadcast(&event_queue->c);
  mtx_unlock(&event_queue->m);
}

struct ntt_task_node *ntt_event_queue_pop(struct ntt_event_queue *event_queue) {
  mtx_lock(&event_queue->m);
  for (;;) {
    if (event_queue->head.next != NULL) {
      struct ntt_task_node *node = event_queue->head.next;
      event_queue->head.next = node->next;
      if (node == event_queue->tail) {
        event_queue->tail = &event_queue->head;
      }
      mtx_unlock(&event_queue->m);
      return node;
    }
    if (event_queue->stopped) {
      mtx_unlock(&event_queue->m);
      return NULL;
    }
    cnd_wait(&event_queue->c, &event_queue->m);
  }
}

void ntt_event_queue_destroy(struct ntt_event_queue *event_queue) {
  cnd_destroy(&event_queue->c);
  mtx_destroy(&event_queue->m);
}
