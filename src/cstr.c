#include "ntt/cstr.h"

#include <stdlib.h>
#include <string.h>

ntt_cstr_t ntt_cstr_from_view(ntt_view_t view)
{
    ntt_cstr_t str;
    str.data = malloc(view.len + 1);
    memcpy(str.data, view.str, view.len);
    str.data[view.len] = '\0';
    return str;
}
