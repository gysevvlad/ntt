#include "ntt/engine.h"

#include <errno.h>
#include <sys/epoll.h>

int ntt_engine_init(struct ntt_engine *engine) {

  engine->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
  if (engine->epoll_fd == -1) {
    return errno;
  }

  return 0;
}
