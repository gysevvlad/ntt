#pragma once

#include "ntt/defs.h"
#include "ntt/export.h"

#include <threads.h>

EXTERN_START

struct ntt_event {
  void (*svc_cb)(void *svc_arg);
  void *svc_arg;
  struct ntt_event *next;
};

struct ntt_event_queue {
  mtx_t mtx;
  cnd_t cnd;
  struct ntt_event head;
  struct ntt_event *tail;
  int stopped;
};

NTT_EXPORT void ntt_event_queue_init(struct ntt_event_queue *event_queue);

NTT_EXPORT void ntt_event_queue_push(struct ntt_event_queue *event_queue,
                                     struct ntt_event *event);

NTT_EXPORT void ntt_event_queue_stop(struct ntt_event_queue *event_queue);

NTT_EXPORT struct ntt_event *
ntt_event_queue_pop(struct ntt_event_queue *event_queue);

NTT_EXPORT void ntt_event_queue_destroy(struct ntt_event_queue *event_queue);

EXTERN_STOP
