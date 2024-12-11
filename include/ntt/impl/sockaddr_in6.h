#pragma once

#include "ntt/char_span.h"
#include "ntt/defs.h"
#include "ntt/view.h"

#include <netinet/in.h>

EXTERN_START

int ntt_sockaddr_in6_from_view(struct sockaddr_in6* addr, ntt_view_t view);

size_t ntt_sockaddr_in6_formatted_size(struct sockaddr_in6* self);

ntt_char_span_t ntt_sockaddr_in6_format_to(struct sockaddr_in6* self,
    ntt_char_span_t buffer);

EXTERN_STOP
