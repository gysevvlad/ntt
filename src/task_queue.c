#include "ntt/task_queue.h"
#include "ntt/defs.h"
#include "ntt/malloc.h"
#include "ntt/mpsc_queue.h"
#include "ntt/node.h"
#include "ntt/event_queue.h"
#include "ntt/work_loop.h"
#include "task_queue_impl.h"
#include "work_loop_impl.h"

#define MAX_CONTINUE_EXECUTION

thread_local struct ntt_task_queue *t_next_queue;
thread_local struct ntt_task_queue *t_curr_queue;
thread_local struct ntt_sq_task *t_curr_task;

void ntt_task_queue_svc(void *arg) {
  struct ntt_task_queue *task_queue = arg;
  struct ntt_node *mpsc_queue_node = NULL;
  int last = 0;
  t_next_queue = NULL;
  t_curr_queue = task_queue;
  do {
    mpsc_queue_node = ntt_mpsc_queue_front(&task_queue->mpsc_queue);
    struct ntt_sq_task *serial_queue_task =
        ntt_container_of(mpsc_queue_node, struct ntt_sq_task, node);
    t_curr_task = serial_queue_task;
    serial_queue_task->svc(serial_queue_task->arg);
    ntt_mpsc_queue_pop(&task_queue->mpsc_queue, &last);
    serial_queue_task->del(serial_queue_task->arg);
  } while (last == 0);
  if (t_next_queue != NULL) {
    // TODO: avoid recursion
    ntt_task_queue_svc(t_next_queue);
  }
  t_curr_queue = NULL;
}

NTT_EXPORT struct ntt_task_queue *
ntt_task_queue_create(struct ntt_work_loop *work_loop) {

  struct ntt_task_queue *task_queue = ntt_malloc(sizeof(struct ntt_task_queue));

  task_queue->refs = 1;

  ntt_mpsc_queue_init(&task_queue->mpsc_queue);

  task_queue->work_loop = ntt_work_loop_acquire(work_loop);

  task_queue->task_node.svc_arg = task_queue;
  task_queue->task_node.svc = ntt_task_queue_svc;

  return task_queue;
}

NTT_EXPORT void ntt_task_queue_release(struct ntt_task_queue *task_queue) {
  size_t prev = atomic_fetch_sub(&task_queue->refs, 1);
  assert(prev > 0 && "internal error: event queue already destroyed");

  if (prev == 1) {
    ntt_work_loop_release(task_queue->work_loop);
    ntt_free(task_queue);
  }
}

NTT_EXPORT void ntt_task_queue_acquire(struct ntt_task_queue *task_queue) {
  size_t prev = atomic_fetch_add(&task_queue->refs, 1);
  assert(prev > 0 && "internal error: event queue already destroyed");
}

void ntt_task_queue_dispatch(struct ntt_task_queue *task_queue,
                             struct ntt_sq_task *node) {
  cds_wfcq_node_init(&node->node.node);
  int first = ntt_mpsc_queue_push(&task_queue->mpsc_queue, &node->node);
  if (first) {
    if (t_curr_queue != NULL) {
      // it's call from ntt_squeue
      if (t_next_queue == NULL) {
        // it's first dispatch call from current queue
        if (ntt_mpsc_queue_lookup(&t_curr_queue->mpsc_queue,
                                  &t_curr_task->node) == 0) {
          // looks like it's last event in current queue
          t_next_queue = task_queue;
          return;
        }
      }
    }
    ntt_event_queue_push(&task_queue->work_loop->event_queue,
                            &task_queue->task_node);
    return;
  }
}
