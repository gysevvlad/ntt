#pragma once

#include "ntt/debug.h"
#include "ntt/defs.h"

#include <stddef.h>

#include <urcu/wfcqueue.h>

EXTERN_START

struct ntt_node {
  struct cds_wfcq_node node;
  DEBUG_FIELD(bool, initialized);
};

inline void ntt_node_init(struct ntt_node *node) {
  cds_wfcq_node_init(&node->node);
  DEBUG_ACTION({ node->initialized = true; });
}

#define ntt_node_from_cds_wfcq_node(node)                                      \
  ((struct ntt_node *)((uint8_t *)(node)-offsetof(struct ntt_node, node)))

EXTERN_STOP
