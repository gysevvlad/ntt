#include "ntt/task.h"

#include "ntt/node.h"

typedef struct ntt_task_node {
  ntt_node_t node;
  ntt_task_cb_t *cb;
  char payload[48];
} ntt_task_node_t;
