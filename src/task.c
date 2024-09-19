#include "./task_impl.h"
#include "./util.h"

#include "ntt/defs.h"

#include <assert.h>
#include <stdlib.h>

ntt_task_t *ntt_make_task(ntt_task_cb_t *cb) {
  ntt_task_node_t *task_node = malloc(sizeof(ntt_task_node_t));
  assert(ntt_is_aligned(&task_node->payload, NTT_TASK_PAYLOAD_ALIGN) &&
         "ntt exception: wrong assumption about task payload alignment");
  task_node->cb = cb;
  return &task_node->payload;
}

void ntt_do_task(ntt_task_t *task) {
  ntt_task_node_t *task_node = ntt_container_of(task, ntt_task_node_t, payload);
  task_node->cb(&task_node->payload);
}

void ntt_free_task(void *task) {
  free(ntt_container_of(task, ntt_task_node_t, payload));
}
