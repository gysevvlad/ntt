#include "./sockaddr_impl.h"

#define NTT_INVALID_SOCKET -1

struct ntt_socket {
    int fd;
    struct ntt_sockaddr addr;
};
