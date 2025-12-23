#pragma once

#include <ntt/defs.h>

#include <stdint.h>

EXTERN_START

typedef struct ntt_socket ntt_socket_t;
typedef struct ntt_sockaddr ntt_sockaddr_t;
typedef struct ntt_loop ntt_loop_t;

NTT_EXPORT ntt_socket_t* ntt_socket_crete();

NTT_EXPORT int ntt_socket_open(ntt_socket_t* self, const ntt_sockaddr_t* sockaddr);

NTT_EXPORT void ntt_socket_delete(ntt_socket_t* self);

NTT_EXPORT void ntt_socket_connect(ntt_socket_t* self, ntt_loop_t* loop);

EXTERN_STOP
