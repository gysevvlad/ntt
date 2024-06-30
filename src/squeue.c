#include "ntt/squeue.h"
#include "ntt/defs.h"
#include "ntt/mpsc_queue.h"
#include "ntt/node.h"
#include "ntt/task_queue.h"

#define MAX_CONTINUE_EXECUTION



void ntt_sq_svc(void *arg) {
  struct ntt_squeue *serial_queue = arg;
  struct ntt_node *mpsc_queue_node = NULL;
  int last = 0;
  do {
    mpsc_queue_node = ntt_mpsc_queue_front(&serial_queue->mpsc_queue);
    struct ntt_sq_task *serial_queue_task =
        ntt_container_of(mpsc_queue_node, struct ntt_sq_task, node);
    serial_queue_task->svc(serial_queue_task->arg);
    ntt_mpsc_queue_pop(&serial_queue->mpsc_queue, &last);
  } while (last == 0);
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
    ntt_task_queue_push(sq->task_queue, &sq->task_node);
  }
}

void ntt_squeue_destroy(struct ntt_squeue *queue) {}
