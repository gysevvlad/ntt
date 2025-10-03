#include "ntt/ec.h"

#include <pthread.h>
#include <string.h>

static pthread_mutex_t ntt_system_error_category_mtx = PTHREAD_MUTEX_INITIALIZER;

ntt_cstr_t ntt_system_error_category_msg(int ec)
{
    ntt_cstr_t msg;
    pthread_mutex_lock(&ntt_system_error_category_mtx);
    msg = ntt_cstr_dup(strerror(ec));
    pthread_mutex_unlock(&ntt_system_error_category_mtx);
    return msg;
}

const char* ntt_system_error_category_name()
{
    return "system";
}

const ntt_ec_category_tbl_t ntt_system_error_category_tbl = {
    .msg = ntt_system_error_category_msg,
    .name = ntt_system_error_category_name,
};
