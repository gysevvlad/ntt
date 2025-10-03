#include "ntt/event.h"
#include "ntt/signal_source.h"
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <errno.h>
#include <fcntl.h>
#include <ntt/ntt.h>

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

typedef struct app {
    ntt_reader_t* stdin_reader;
} app_t;

void ntt_stdin_reader_svc(ntt_reader_t* self, void* ctx, uint8_t* data, size_t size)
{
    printf("recv %zu bytes\n", size);
}

void ntt_stdin_reader_stopped(ntt_reader_t* self, void* ctx, int rc)
{
    printf("reader stopped\n");
}

static const ntt_reader_vtbl_t g_stdin_reader = {
    .name      = "stdin-reader",
    .on_data   = ntt_stdin_reader_svc,
    .on_cancel = ntt_stdin_reader_stopped,
};

void app_init(app_t* self)
{
    self->stdin_reader = ntt_reader_create(&g_stdin_reader, self);
}

static app_t* app_from_ctx(void* ctx) { return ctx; }

void ntt_loop_started(ntt_loop_t* loop, void* ctx)
{
    app_t* app = app_from_ctx(ctx);

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
        abort();
    }

    ntt_reader_start(app->stdin_reader, STDIN_FILENO, loop);
}

static const ntt_loop_vptr_t loop_vtbl = {
    .name    = "example-03",
    .started = ntt_loop_started,
};

int main(int argc, char* argv[])
{
    app_t app;
    app_init(&app);
    ntt_loop_svc(&loop_vtbl, &app, 4);
    return 0;
}
