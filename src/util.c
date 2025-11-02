#include "ntt/util.h"
#include "ntt/char_span.h"
#include "ntt/view.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ntt_long_from_view(long* value, ntt_view_t view)
{
    if (!(0 < view.len && view.len < 32)) {
        return 0;
    }
    // TODO: avoid copy to internal buffer
    char buffer[32];
    memcpy(buffer, view.str, view.len);
    buffer[view.len] = '\0';
    char* endptr     = NULL;
    errno            = 0;
    *value           = strtol(buffer, &endptr, 10);
    return *endptr == '\0' && errno == 0 ? 1 : 0;
}

int ntt_long_from_cstr(long* value, const char* cstr)
{
    return ntt_long_from_view(value, ntt_view_from_cstr(cstr));
}

int ntt_unsigned_short_from_view(unsigned short* value, ntt_view_t view)
{
    long v;
    if (ntt_long_from_view(&v, view)) {
        if (0 <= v && v <= USHRT_MAX) {
            *value = v;
            return 1;
        }
    }
    return 0;
}

int ntt_unsigned_short_from_cstr(unsigned short* value, const char* cstr)
{
    return ntt_unsigned_short_from_view(value, ntt_view_from_cstr(cstr));
}

int ntt_unsigned_short_formatted_size(unsigned short value)
{
    return snprintf(NULL, 0, "%i", value);
}

size_t ntt_unsigned_short_format_to(unsigned short value, char* buf, size_t len)
{
    char temp[32];
    size_t l = 0;
    do {
        temp[l++] = value % 10;
        value     = value / 10;
    } while (value);

    size_t size = l;
    if (size > len) {
        size = len;
    }

    if (buf != NULL) {
        size_t i;
        for (i = 0; i < size; ++i) {
            buf[i] = '0' + temp[--l];
        }
    }

    return size;
}
