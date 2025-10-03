#include "ntt/view.h"

#include <string.h>

int ntt_view_split_by_char(
    ntt_view_t view,
    char delimeter,
    ntt_view_t* head,
    ntt_view_t* tail)
{
    size_t i;
    for (i = 0; i < view.len; ++i) {
        if (view.str[i] == delimeter) {
            head->len = i;
            head->str = view.str;
            tail->len = view.len - i - 1;
            tail->str = view.str + i + 1;
            return 1;
        }
    }
    return 0;
}

ntt_view_t ntt_view_from_cstr(const char* cstr)
{
    ntt_view_t view = { .len = strlen(cstr), .str = cstr };
    return view;
}