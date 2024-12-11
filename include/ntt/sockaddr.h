#pragma once

#include "ntt/char_span.h"
#include "ntt/defs.h"
#include "ntt/export.h"
#include "ntt/view.h"

#include <stddef.h>

EXTERN_START

/**
 * @brief Socket address: ipv4/ipv6/uds
 */
typedef struct ntt_sockaddr ntt_sockaddr_t;

/**
 * @brief Create socket address from c-str.
 *
 * @details
 *  It is implemented as serial parsing str as:
 *    1. ipv4 address   <ipv4_address>:<port>
 *    2. ipv6 address   [<ipv6_address>]:<port>
 *    3. uds            (\/[^\/]+)+\.sock
 *
 * @return ntt_sockaddr
 */
NTT_EXPORT ntt_sockaddr_t* ntt_sockaddr_make_from_view(ntt_view_t view);

NTT_EXPORT size_t ntt_sockaddr_formatted_size(const ntt_sockaddr_t* self);

NTT_EXPORT ntt_char_span_t ntt_sockaddr_format_to(const ntt_sockaddr_t* self,
    ntt_char_span_t buffer);

NTT_EXPORT void ntt_sockaddr_acquire(ntt_sockaddr_t* self);

NTT_EXPORT void ntt_sockaddr_release(ntt_sockaddr_t* self);

NTT_EXPORT int ntt_create_and_bind_socket(ntt_sockaddr_t* addr);

EXTERN_STOP
