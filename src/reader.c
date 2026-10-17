#include "./reader.h"
#include <ntt/queue.h>
#include "./malloc.h"

#include <errno.h>
#include <unistd.h>

static void ntt_reader_readable(ntt_event_handle_t event, void* ctx, int fd)
{
    ntt_reader_t* self = ctx;

    for (;;) {
        ssize_t rc = read(fd, self->data, sizeof(self->data));

        if (rc > 0) {
            self->vtbl->on_data(self, self->ctx, self->data, (size_t)rc);
            continue;
        }

        if (rc == 0) {
            // Nothing more will ever arrive, so the reader is done.
            ntt_event_cancel(ntt_queue_loop(self->queue), event);
            return;
        }

        if (errno == EINTR) {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }

        self->ec = errno;
        ntt_event_cancel(ntt_queue_loop(self->queue), event);
        return;
    }
}

/**
 * @brief The event is gone, so nothing of this reader is in flight.
 *
 * The descriptor belongs to whoever started the reader, so it is left open.
 */
static void ntt_reader_cancelled(void* ctx, int fd)
{
    (void)fd;

    ntt_reader_t* self = ctx;

    self->vtbl->on_cancel(self, self->ctx, self->ec);
}

const static ntt_event_vtbl_t g_ntt_reader_vtbl = {
    .name      = "ntt_reader",
    .readable  = ntt_reader_readable,
    .writable  = NULL,
    .cancelled = ntt_reader_cancelled,
};

void ntt_reader_init(
    ntt_reader_t* self,
    const ntt_reader_vtbl_t* vtbl,
    void* ctx)
{
    self->vtbl  = vtbl;
    self->ctx   = ctx;
    self->queue = NULL;
    self->event = NTT_EVENT_HANDLE_NONE;
    self->ec    = 0;
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
    ntt_queue_t* queue)
{
    self->queue = queue;

    ntt_event_params_t params = {
        .fd         = fd,
        .vtbl       = &g_ntt_reader_vtbl,
        .ctx        = self,
        .interest   = NTT_INTEREST_READABLE,
        .read_queue = queue,
        .handle     = &self->event,
    };

    // Nothing of the reader may be touched after this: the handler can reach
    // the end of the input and the cancellation can free the reader before it
    // returns. The handle is written by the call itself, before that is
    // possible.
    ntt_event_start(&params);
}

void ntt_reader_cancel(
    ntt_reader_t* self)
{
    if (self->queue == NULL) {
        return;
    }

    ntt_event_cancel(ntt_queue_loop(self->queue), self->event);
}

void ntt_reader_delete(
    ntt_reader_t* self)
{
    ntt_free(self);
}
