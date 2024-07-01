#include "ntt/task_queue_dep.h"
#include "ntt/malloc.h"
#include <assert.h>
#include <threads.h>

void ntt_task_queue_dep_init(struct ntt_task_queue_dep *task_queue_dep) {
  mtx_init(&task_queue_dep->m, mtx_plain);
  cnd_init(&task_queue_dep->c);
  task_queue_dep->head.next = NULL;
  task_queue_dep->tail = &task_queue_dep->head;
  task_queue_dep->stopped = 0;
}

void ntt_task_queue_dep_push(struct ntt_task_queue_dep *task_queue_dep,
                             struct ntt_task_node *task_node) {
  task_node->next = NULL;
  mtx_lock(&task_queue_dep->m);
  task_queue_dep->tail->next = task_node;
  task_queue_dep->tail = task_node;
  mtx_unlock(&task_queue_dep->m);
  cnd_signal(&task_queue_dep->c);
}

void ntt_task_queue_dep_stop(struct ntt_task_queue_dep *task_queue_dep) {
  mtx_lock(&task_queue_dep->m);
  assert(task_queue->stopped == 0 &&
         "internal error: task queue already stopped");
  task_queue_dep->stopped = 1;
  cnd_broadcast(&task_queue_dep->c);
  mtx_unlock(&task_queue_dep->m);
}

struct ntt_task_node *
ntt_task_queue_dep_pop(struct ntt_task_queue_dep *task_queue_dep) {
  mtx_lock(&task_queue_dep->m);
  for (;;) {
    if (task_queue_dep->head.next != NULL) {
      struct ntt_task_node *node = task_queue_dep->head.next;
      task_queue_dep->head.next = node->next;
      if (node == task_queue_dep->tail) {
        task_queue_dep->tail = &task_queue_dep->head;
      }
      mtx_unlock(&task_queue_dep->m);
      return node;
    }
    if (task_queue_dep->stopped) {
      mtx_unlock(&task_queue_dep->m);
      return NULL;
    }
    cnd_wait(&task_queue_dep->c, &task_queue_dep->m);
  }
}

void ntt_task_queue_dep_destroy(struct ntt_task_queue_dep *task_queue_dep) {
  cnd_destroy(&task_queue_dep->c);
  mtx_destroy(&task_queue_dep->m);
}
