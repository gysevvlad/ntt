#include "ntt/impl/reader.h"
#include "ntt/event_source.h"
#include "ntt/reader.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#define NTT_READER_FULL_SIZE 4096
#define NTT_READER_BUFFER_SIZE NTT_READER_FULL_SIZE - sizeof(ntt_reader_t)

int ntt_reader_wakeup_svc(ntt_event_source_t* src, void* ctx, int events)
{
    ntt_reader_t* self = ctx;

    assert(events == NTT_READ_EVENT);

    int rc = 0;
    int transferred = 0;
    do {
        transferred += rc;
        rc = read(src->fd, self->data, NTT_READER_BUFFER_SIZE);
        if (rc > 0) {
            self->listener_tbl->on_data(self->listener_ctx, self, self->data, rc);
        }
    } while (rc > 0 || (rc == -1 && errno == EINTR));

    if (rc == -1) {
        int ec = errno;
        if (ec == EWOULDBLOCK || ec == EAGAIN) {
            return 0;
        }
        return ec;
    }

    if (rc == 0) {
        ntt_event_source_cancel(src);
        return 0;
    }

    return 0;
}

void ntt_reader_on_stop(ntt_event_source_t* src, void* ctx)
{
    (void)src;
    ntt_reader_t* self = ctx;
    self->listener_tbl->on_stop(self->listener_ctx, self, self->event.ec);
}

const ntt_event_handler_tbl_t g_pipe_reader_event_handler = {
    .name = "reader",
    .on_ready_cb = ntt_reader_wakeup_svc,
    .on_del_cb = ntt_reader_on_stop,
};

ntt_reader_t* ntt_reader_create(
    ntt_pool_t* pool,
    int fd,
    const ntt_reader_listener_tbl_t* listener_tbl,
    void* listener_ctx)
{
    ntt_reader_t* self = malloc(sizeof(ntt_reader_t) + (NTT_READER_FULL_SIZE - sizeof(ntt_reader_t)));

    if (self == NULL) {
        return NULL;
    }

    ntt_event_source_init(&self->event, pool, &g_pipe_reader_event_handler, self, fd, NTT_READ_EVENT);
    self->listener_tbl = listener_tbl;
    self->listener_ctx = listener_ctx;

    return self;
}

void ntt_reader_start(
    ntt_reader_t* self)
{
    ntt_event_source_start(&self->event);
}

void ntt_reader_stop(
    ntt_reader_t* self)
{
    ntt_event_source_cancel(&self->event);
}

void ntt_reader_destroy(
    ntt_reader_t* self)
{
    ntt_event_source_deinit(&self->event);
    close(self->event.fd);
    free(self);
}
