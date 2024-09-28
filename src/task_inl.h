#include "./node.h"
#include "./util.h"

#include "ntt/task.h"

#include <assert.h>
#include <stdlib.h>

typedef struct ntt_task_node {
  ntt_node_t node;
  ntt_task_cb_t *cb;
  char payload[40];
} ntt_task_node_t;

static inline ntt_task_node_t *ntt_make_task_impl(ntt_task_cb_t *cb) {
  ntt_task_node_t *task_node = malloc(sizeof(ntt_task_node_t));
  assert(ntt_is_aligned(&task_node->payload, NTT_TASK_PAYLOAD_ALIGN) &&
         "ntt exception: wrong assumption about task payload alignment");
  task_node->cb = cb;
  return task_node;
}

static inline void ntt_do_task_inl(ntt_task_t *task) {
  ntt_task_node_t *task_node = ntt_container_of(task, ntt_task_node_t, payload);
  task_node->cb(&task_node->payload);
}

static inline void ntt_free_task_inl(void *task) {
  free(ntt_container_of(task, ntt_task_node_t, payload));
}
