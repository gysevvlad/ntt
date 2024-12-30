#include "ntt/impl/session.h"
#include "ntt/session.h"

ntt_session_backend_t* ntt_session_backend_create(
    ntt_pool_t* pool,
    int sock,
    const ntt_session_listener_tbl_t* listener_tbl,
    void* listener_ctx)
{
}

void ntt_session_backend_start(
    ntt_session_t* self)
{
}

void ntt_session_backend_stop(
    ntt_session_t* self)
{
}

void ntt_session_backend_destroy(
    ntt_session_t* self)
{
}
