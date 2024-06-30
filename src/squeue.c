#include "ntt/squeue.h"
#include "ntt/defs.h"
#include "ntt/mpsc_queue.h"
#include "ntt/node.h"
#include "ntt/task_queue.h"

#define MAX_CONTINUE_EXECUTION

thread_local struct ntt_squeue *t_next_queue;
thread_local struct ntt_squeue *t_curr_queue;
thread_local struct ntt_sq_task *t_curr_task;

void ntt_sq_svc(void *arg) {
  struct ntt_squeue *serial_queue = arg;
  struct ntt_node *mpsc_queue_node = NULL;
  int last = 0;
  t_next_queue = NULL;
  t_curr_queue = serial_queue;
  do {
    mpsc_queue_node = ntt_mpsc_queue_front(&serial_queue->mpsc_queue);
    struct ntt_sq_task *serial_queue_task =
        ntt_container_of(mpsc_queue_node, struct ntt_sq_task, node);
    t_curr_task = serial_queue_task;
    serial_queue_task->svc(serial_queue_task->arg);
    ntt_mpsc_queue_pop(&serial_queue->mpsc_queue, &last);
    serial_queue_task->del(serial_queue_task->arg);
  } while (last == 0);
  if (t_next_queue != NULL) {
    // TODO: avoid recursion
    ntt_sq_svc(t_next_queue);
  }
  t_curr_queue = NULL;
}

void ntt_squeue_init(struct ntt_squeue *squeue,
                     struct ntt_task_queue *task_queue) {
  ntt_mpsc_queue_init(&squeue->mpsc_queue);
  squeue->task_queue = task_queue;
  squeue->task_node.arg = squeue;
  squeue->task_node.svc = ntt_sq_svc;
}

void ntt_squeue_dispatch(struct ntt_squeue *sq, struct ntt_sq_task *node) {
  cds_wfcq_node_init(&node->node.node);
  int first = ntt_mpsc_queue_push(&sq->mpsc_queue, &node->node);
  if (first) {
    if (t_curr_queue != NULL) {
      // it's call from ntt_squeue
      if (t_next_queue == NULL) {
        // it's first dispatch call from current queue
        if (ntt_mpsc_queue_lookup(&t_curr_queue->mpsc_queue,
                                  &t_curr_task->node) == 0) {
          // looks like it's last event in current queue
          t_next_queue = sq;
          return;
        }
      }
    }
    ntt_task_queue_push(sq->task_queue, &sq->task_node);
    return;
  }
}

void ntt_squeue_destroy(struct ntt_squeue *queue) {}
