#pragma once

#include "ntt/defs.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <urcu/wfcqueue.h>

#define NTT_MAX_EPOLL_EVENTS 128
#define NTT_MAX_EPOLL_ACTIONS 128

EXTERN_START

struct ntt_event_loop;

struct ntt_epoll_fd {
  void (*on_events)(struct ntt_epoll_fd *epoll_fd, int events);
  void (*on_action)(struct ntt_epoll_fd *epoll_fd,
                    struct ntt_event_loop *event_loop);
  struct cds_wfcq_node actions_node;
};

struct ntt_event_loop {
  int epoll_fd;
  struct epoll_event events[NTT_MAX_EPOLL_EVENTS];
  /// actions list
  struct cds_wfcq_head actions_head;
  struct cds_wfcq_tail actions_tail;
  int actions_event_fd;
  struct ntt_epoll_fd actions_fd;
  uint64_t active_events;
};

void ntt_event_loop_init(struct ntt_event_loop *event_loop);

void ntt_event_loop_action(struct ntt_event_loop *event_loop,
                           struct ntt_epoll_fd *epoll_fd);

void ntt_event_loop_run_once(struct ntt_event_loop *event_loop);

void ntt_event_loop_run(struct ntt_event_loop *event_loop);

EXTERN_STOP
