#pragma once

#include "ntt/defs.h"
#include "ntt/export.h"
#include "ntt/view.h"

EXTERN_START

NTT_EXPORT char *ntt_cstr_from_view(ntt_view_t view);

NTT_EXPORT void ntt_cstr_free(char* cstr);

EXTERN_STOP
