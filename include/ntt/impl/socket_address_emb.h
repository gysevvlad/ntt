#pragma once

#include <sys/socket.h>

typedef struct ntt_socket_address_emb {
  struct sockaddr_storage storage;
} ntt_socket_address_emd_t;
