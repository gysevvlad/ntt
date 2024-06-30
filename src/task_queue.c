#include "ntt/task_queue.h"
#include "ntt/malloc.h"
#include <assert.h>
#include <threads.h>

void ntt_task_queue_init(struct ntt_task_queue *task_queue) {
  mtx_init(&task_queue->m, mtx_plain);
  cnd_init(&task_queue->c);
  task_queue->head.next = NULL;
  task_queue->tail = &task_queue->head;
  task_queue->stopped = 0;
}

void ntt_task_queue_push(struct ntt_task_queue *task_queue,
                         struct ntt_task_node *task_node) {
  task_node->next = NULL;
  mtx_lock(&task_queue->m);
  task_queue->tail->next = task_node;
  task_queue->tail = task_node;
  mtx_unlock(&task_queue->m);
  cnd_signal(&task_queue->c);
}

void ntt_task_queue_stop(struct ntt_task_queue *task_queue) {
  mtx_lock(&task_queue->m);
  assert(task_queue->stopped == 0 &&
         "internal error: task queue already stopped");
  task_queue->stopped = 1;
  cnd_broadcast(&task_queue->c);
  mtx_unlock(&task_queue->m);
}

struct ntt_task_node *ntt_task_queue_pop(struct ntt_task_queue *task_queue) {
  mtx_lock(&task_queue->m);
  for (;;) {
    if (task_queue->head.next != NULL) {
      struct ntt_task_node *node = task_queue->head.next;
      task_queue->head.next = node->next;
      if (node == task_queue->tail) {
        task_queue->tail = &task_queue->head;
      }
      mtx_unlock(&task_queue->m);
      return node;
    }
    if (task_queue->stopped) {
      mtx_unlock(&task_queue->m);
      return NULL;
    }
    cnd_wait(&task_queue->c, &task_queue->m);
  }
}

void ntt_task_queue_destroy(struct ntt_task_queue *task_queue) {
  cnd_destroy(&task_queue->c);
  mtx_destroy(&task_queue->m);
}
