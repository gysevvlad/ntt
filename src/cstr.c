#include "ntt/cstr.h"

#include <stdlib.h>
#include <string.h>

char* ntt_cstr_from_view(ntt_view_t view)
{
    char* v = malloc(view.len + 1);
    memcpy(v, view.str, view.len);
    v[view.len] = '\0';
    return v;
}

void ntt_cstr_free(char* cstr) { free(cstr); }