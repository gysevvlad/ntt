#include "./reader.h"
#include "ntt/impl/malloc.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

void ntt_reader_event_svc_cb(ntt_event_t* event, void* ctx, ntt_interest_t interest)
{
    ntt_reader_t* self = ntt_container_of(event, ntt_reader_t, event);

    for (;;) {
        int rc = read(event->raw_event.fd, self->data, 1024);
        if (rc == -1) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            // TODO: something going wrong,
            //       cancel event and provide cancel reason - ???
            abort();
            return;
        }
        if (rc == 0) {
            // TODO: provide cancel reason - EOF
            ntt_event_cancel(event);
            return;
        }
        self->vtbl->on_data(self, ctx, self->data, rc);
    }
}

void ntt_reader_event_stopped_cb(ntt_event_t* event, void* ctx, ntt_ec_t ec)
{
    ntt_reader_t* self = ntt_container_of(event, ntt_reader_t, event);

    self->vtbl->on_cancel(self, ctx, ec.ec);
}

const static ntt_event_vtbl_t g_ntt_reader_vtbl = {
    .name                 = "ntt_reader",
    .ntt_event_svc_cb     = ntt_reader_event_svc_cb,
    .ntt_event_stopped_cb = ntt_reader_event_stopped_cb,
};

void ntt_reader_init(
    ntt_reader_t* self,
    const ntt_reader_vtbl_t* vtbl,
    void* ctx)
{
    self->vtbl = vtbl;
    self->ctx  = ctx;
    ntt_event_init(&self->event, &g_ntt_reader_vtbl, self, NTT_INTEREST_READABLE);
}

ntt_reader_t* ntt_reader_create(
    const ntt_reader_vtbl_t* vtbl,
    void* ctx)
{
    ntt_reader_t* self = ntt_malloc(sizeof(ntt_reader_t));

    if (self == NULL) {
        return NULL;
    }

    ntt_reader_init(self, vtbl, ctx);

    return self;
}

void ntt_reader_start(
    ntt_reader_t* self,
    int fd,
    ntt_loop_t* loop)
{
    ntt_event_start(&self->event, fd, loop);
}

void ntt_reader_cancel(
    ntt_reader_t* self)
{
    ntt_event_cancel(&self->event);
}

void ntt_reader_delete(
    ntt_reader_t* self)
{
    ntt_free(self);
}
