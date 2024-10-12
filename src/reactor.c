#include "ntt/impl/reactor.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>

void ntt_reactor_init(ntt_reactor_t *reactor) {
  reactor->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
  if (reactor->epoll_fd == -1) {
    abort();
  }
  reactor->epoll_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE);
  if (reactor->epoll_fd == -1) {
    abort();
  }
}
