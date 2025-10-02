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
    ntt_signal_source_t* signal_source;
    ntt_event_t* stdin_event;
} app_t;

static app_t* app_from_ctx(void* ctx) { return ctx; }

static void app_signal_raised(ntt_signal_source_t* signal_source, void* ctx, int signal)
{
    app_t* app = app_from_ctx(ctx);

    if (signal == SIGINT) {
        printf("got SIGINT\n");
    } else {
        printf("got signal %i\n", signal);
    }

    ntt_event_cancel(app->stdin_event);
    ntt_signal_source_stop(signal_source);
}

static void app_signal_listener_stopped(ntt_signal_source_t* signal_source, void* ctx)
{
}

static const ntt_signal_listener_vtbl_t g_app_signal_source_vtbl = {
    .name    = "signal-listener",
    .raised  = app_signal_raised,
    .stopped = app_signal_listener_stopped,
};

void ntt_stdin_reader_svc(ntt_event_t* self, void* ctx, ntt_interest_t interest)
{
    app_t* app = ctx;

    char buffer[1024];
    for (;;) {
        printf("before\n");
        int rc = read(STDIN_FILENO, buffer, 1024);
        printf("after\n");
        if (rc == -1) {
            if (errno == EINTR) {
                continue;
            }
            if (errno != EWOULDBLOCK || errno != EAGAIN) {
                assert(0);
            }
            return;
        }
        if (rc == 0) {
            ntt_event_cancel(app->stdin_event);
        }
        printf("get %i bytes\n", rc);
    }
}

void ntt_stdin_reader_stopped(ntt_event_t* self, void* ctx, ntt_ec_t ec)
{
    printf("reader stopped\n");
}

static const ntt_event_vtbl_t g_stdin_reader = {
    .name                 = "stdin-reader",
    .ntt_event_svc_cb     = ntt_stdin_reader_svc,
    .ntt_event_stopped_cb = ntt_stdin_reader_stopped,
};

void ntt_loop_started(ntt_loop_t* loop, void* ctx)
{
    app_t* app = app_from_ctx(ctx);

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
        abort();
    }

    app->stdin_event = ntt_event_create(&g_stdin_reader, app, STDIN_FILENO, NTT_INTEREST_READABLE);
    ntt_event_start(app->stdin_event, loop);

    ntt_signal_source_start(app_from_ctx(ctx)->signal_source, loop);
    printf("waiting signal...\n");
}

static const ntt_loop_vptr_t loop_vtbl = {
    .name    = "example-02",
    .started = ntt_loop_started,
};

int main(int argc, char* argv[])
{
    ntt_signal_setup(SIGINT);
    app_t app;
    app.signal_source = ntt_signal_source_create(&g_app_signal_source_vtbl, &app, SIGINT);
    ntt_loop_svc(&loop_vtbl, &app, 4);
    ntt_signal_source_destroy(app.signal_source);
    printf("app stopped\n");
    ntt_signal_revert(SIGTERM);
    return 0;
}
