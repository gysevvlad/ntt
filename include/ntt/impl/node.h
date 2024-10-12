#pragma once

typedef struct ntt_node {
  struct ntt_node *next;
  struct ntt_node *prev;
} ntt_node_t;
