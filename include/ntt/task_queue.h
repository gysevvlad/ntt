#pragma once

#include "ntt/defs.h"

#include <threads.h>

EXTERN_START

struct ntt_task_node {
  void (*svc)(void *);
  void *arg;
  struct ntt_task_node *next;
};

struct ntt_task_queue {
  mtx_t m;
  cnd_t c;
  struct ntt_task_node head;
  struct ntt_task_node *tail;
  int stopped;
};

void ntt_task_queue_init(struct ntt_task_queue *task_queue);

void ntt_task_queue_push(struct ntt_task_queue *task_queue,
                         struct ntt_task_node *node);

void ntt_task_queue_stop(struct ntt_task_queue *task_queue);

struct ntt_task_node *ntt_task_queue_pop(struct ntt_task_queue *task_queue);

void ntt_task_queue_destroy(struct ntt_task_queue *task_queue);

EXTERN_STOP
