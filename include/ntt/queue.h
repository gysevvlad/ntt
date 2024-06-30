#pragma once

#include "ntt/defs.h"

EXTERN_START

struct ntt_queue {
  struct ntt_second_queue *second_queue;
};

void ntt_queue_init(struct ntt_queue* queue, struct ntt_queue* parent_queue);

EXTERN_STOP
