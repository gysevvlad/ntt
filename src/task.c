#include "ntt/task.h"

void ntt_task_init(struct ntt_task *task, void *context,
                               ntt_callback *callback) {
  task->context = context;
  task->callback = callback;
}
