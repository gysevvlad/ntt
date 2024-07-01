#pragma once

#include "ntt/mpsc_queue.h"
#include "ntt/event_queue.h"
#include "work_loop_impl.h"

#include <stdatomic.h>

struct ntt_task_queue {
  atomic_size_t refs;
  struct ntt_mpsc_queue mpsc_queue;
  struct ntt_work_loop *work_loop;
  struct ntt_task_node task_node;
};
