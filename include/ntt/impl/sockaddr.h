#include <sys/socket.h>

struct ntt_sockaddr {
    size_t refs;
    struct sockaddr_storage storage;
};
