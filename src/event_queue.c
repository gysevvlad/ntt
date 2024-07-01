#include "ntt/event_queue.h"
#include "event_queue_impl.h"
#include "ntt/defs.h"
#include "ntt/malloc.h"
#include "ntt/mpsc_queue.h"
#include "ntt/node.h"
#include "ntt/task_queue.h"
#include "ntt/work_loop.h"
#include "work_loop_impl.h"

#define MAX_CONTINUE_EXECUTION

thread_local struct ntt_event_queue *t_next_queue;
thread_local struct ntt_event_queue *t_curr_queue;
thread_local struct ntt_sq_task *t_curr_task;

void ntt_event_queue_svc(void *arg) {
  struct ntt_event_queue *serial_queue = arg;
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
    ntt_event_queue_svc(t_next_queue);
  }
  t_curr_queue = NULL;
}

NTT_EXPORT struct ntt_event_queue *
ntt_event_queue_create(struct ntt_work_loop *work_loop) {

  struct ntt_event_queue *event_queue =
      ntt_malloc(sizeof(struct ntt_event_queue));

  event_queue->refs = 1;

  ntt_mpsc_queue_init(&event_queue->mpsc_queue);

  event_queue->work_loop = ntt_work_loop_acquire(work_loop);

  event_queue->task_node.svc_arg = event_queue;
  event_queue->task_node.svc = ntt_event_queue_svc;

  return event_queue;
}

NTT_EXPORT void ntt_event_queue_release(struct ntt_event_queue *event_queue) {
  size_t prev = atomic_fetch_sub(&event_queue->refs, 1);
  assert(prev > 0 && "internal error: event queue already destroyed");

  if (prev == 1) {
    ntt_work_loop_release(event_queue->work_loop);
    ntt_free(event_queue);
  }
}

NTT_EXPORT void ntt_event_queue_acquire(struct ntt_event_queue *event_queue) {
  size_t prev = atomic_fetch_add(&event_queue->refs, 1);
  assert(prev > 0 && "internal error: event queue already destroyed");
}

void ntt_event_queue_dispatch(struct ntt_event_queue *sq,
                              struct ntt_sq_task *node) {
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
    ntt_task_queue_push(&sq->work_loop->task_queue, &sq->task_node);
    return;
  }
}
