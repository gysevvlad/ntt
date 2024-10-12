#pragma once

#include "ntt/impl/list.h"
#include "ntt/impl/task.h"

#include <sys/epoll.h>

typedef struct ntt_reactor_task {
  int op;
  int fd;
  struct epoll_event event;
} ntt_reactor_task_t;

typedef struct ntt_reactor {
  int event_fd;
  int epoll_fd;
  ntt_task_node_t task_node;
} ntt_reactor_t;

void ntt_reactor_init(ntt_reactor_t *reactor);
