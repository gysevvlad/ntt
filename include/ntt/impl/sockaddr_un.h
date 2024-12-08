#pragma once

#include "ntt/char_span.h"
#include "ntt/defs.h"
#include "ntt/export.h"
#include "ntt/view.h"

#include <sys/un.h>

EXTERN_START

NTT_EXPORT int ntt_sockaddr_un_from_view(struct sockaddr_un *self,
                                         ntt_view_t view);

NTT_EXPORT size_t ntt_sockaddr_un_formatted_size(struct sockaddr_un *self);

NTT_EXPORT ntt_char_span_t ntt_sockaddr_un_format_to(struct sockaddr_un *self,
                                                     ntt_char_span_t buffer);

EXTERN_STOP
