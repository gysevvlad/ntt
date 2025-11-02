#pragma once

#include "ntt/char_span.h"
#include "ntt/defs.h"
#include "ntt/export.h"
#include "ntt/view.h"

EXTERN_START

NTT_EXPORT int ntt_long_from_view(long* value, ntt_view_t view);

NTT_EXPORT int ntt_long_from_cstr(long* value, const char* cstr);

NTT_EXPORT int ntt_unsigned_short_from_view(unsigned short* value,
    ntt_view_t view);

NTT_EXPORT int ntt_unsigned_short_from_cstr(unsigned short* value,
    const char* cstr);

NTT_EXPORT int ntt_unsigned_short_formatted_size(unsigned short value);

NTT_EXPORT size_t ntt_unsigned_short_format_to(unsigned short value, char* buf, size_t len);

EXTERN_STOP
