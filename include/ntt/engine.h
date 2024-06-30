#pragma once

struct ntt_engine {
  int epoll_fd;
};

int ntt_engine_init(struct ntt_engine *engine);
