#pragma once

#include "ntt/defs.h"

#include <stddef.h>
#include <stdint.h>

EXTERN_START

typedef struct ntt_sockaddr ntt_sockaddr_t;

NTT_EXPORT ntt_sockaddr_t* ntt_sockaddr_create_from_ipv4_and_port(const char* ipv4, uint16_t port);

NTT_EXPORT ntt_sockaddr_t* ntt_sockaddr_create_from_ipv6_and_port(const char* ipv6, uint16_t port);

NTT_EXPORT size_t ntt_sockaddr_format_to(const ntt_sockaddr_t* self, char* buffer, size_t len);

NTT_EXPORT void ntt_sockaddr_delete(ntt_sockaddr_t* self);

EXTERN_STOP
