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
    int stopped;
    ntt_event_t* stdin_event;
} app_t;

void app_init(app_t* self)
{
    self->stopped     = 0;
    self->stdin_event = NULL;
}

static void app_on_signal(ntt_loop_t* loop, void* ctx, int signal)
{
    app_t* app = ctx;
    ntt_event_cancel(app->stdin_event);
}

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

void app_on_start(ntt_loop_t* loop, void* ctx)
{
    app_t* app = ctx;

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
        abort();
    }

    app->stdin_event = ntt_event_create(&g_stdin_reader, app, NTT_INTEREST_READABLE);
    ntt_event_start(app->stdin_event, STDIN_FILENO, loop);
}

int main(int argc, char* argv[])
{
    app_t app;
    app_init(&app);
    ntt_loop_svc(
        ntt_loop_cbs_make(
            app_on_start,
            app_on_signal),
        &app, 4);
    printf("app stopped\n");
    return 0;
}
