#pragma once

#include "ntt/char_span.h"
#include "ntt/defs.h"
#include "ntt/view.h"

#include <netinet/in.h>

EXTERN_START

int ntt_in_addr_from_view(struct in_addr *addr, ntt_view_t str);

size_t ntt_in_addr_formatted_size(struct in_addr *addr);

ntt_char_span_t ntt_in_addr_format_to(struct in_addr *addr,
                                      ntt_char_span_t buffer);

EXTERN_STOP
