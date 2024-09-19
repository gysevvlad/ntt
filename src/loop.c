#include "ntt/loop.h"
#include <pthread.h>

void ntt_loop_actions_on_event(struct ntt_epoll_fd *epoll_fd, int event) {
  struct ntt_event_loop *loop =
      ntt_container_of(epoll_fd, struct ntt_event_loop, actions_fd);
  uint64_t notify;
  read(loop->actions_event_fd, &notify, sizeof(uint64_t));

  int state;

  do {
    struct cds_wfcq_node *node;
    do {
      node = __cds_wfcq_dequeue_with_state_nonblocking(
          &loop->actions_head, &loop->actions_tail, &state);
    } while (node == CDS_WFCQ_WOULDBLOCK);

    struct ntt_epoll_fd *epoll_fd =
        ntt_container_of(node, struct ntt_epoll_fd, actions_node);

    epoll_fd->on_action(epoll_fd, loop);

  } while (state != CDS_WFCQ_STATE_LAST);
}

void ntt_event_loop_init(struct ntt_event_loop *event_loop) {
  event_loop->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
  cds_wfcq_init(&event_loop->actions_head, &event_loop->actions_tail);
  event_loop->actions_event_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  cds_wfcq_node_init(&event_loop->actions_fd.actions_node); // actually not used
  event_loop->actions_fd.on_events = &ntt_loop_actions_on_event;
  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.ptr = &event_loop->actions_fd;
  epoll_ctl(event_loop->epoll_fd, EPOLL_CTL_ADD, event_loop->actions_event_fd,
            &ev);
  event_loop->active_events = 1;
}

void ntt_event_loop_action(struct ntt_event_loop *event_loop,
                           struct ntt_epoll_fd *epoll_fd) {
  if (cds_wfcq_enqueue(&event_loop->actions_head, &event_loop->actions_tail,
                       &epoll_fd->actions_node) == false) {
    uint64_t notify = 1;
    write(event_loop->actions_event_fd, &notify, sizeof(uint64_t));
  }
}

void ntt_event_loop_run_once(struct ntt_event_loop *event_loop) {
  int cnt = epoll_wait(event_loop->epoll_fd, event_loop->events,
                       NTT_MAX_EPOLL_EVENTS, -1);
  for (int i = 0; i < cnt; ++i) {
    struct ntt_epoll_fd *listener = event_loop->events[i].data.ptr;
    listener->on_events(listener, event_loop->events[i].events);
  }
}

void ntt_event_loop_run(struct ntt_event_loop *event_loop) {
  while (event_loop->active_events > 0) {
    int cnt = epoll_wait(event_loop->epoll_fd, event_loop->events,
                         NTT_MAX_EPOLL_EVENTS, -1);
    for (int i = 0; i < cnt; ++i) {
      struct ntt_epoll_fd *listener = event_loop->events[i].data.ptr;
      listener->on_events(listener, event_loop->events[i].events);
    }
  }
}
