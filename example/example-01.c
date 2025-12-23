#include <ntt/ntt.h>

#include <signal.h>
#include <stdio.h>

typedef struct app {
    ntt_signal_source_t* signal_source;
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

void ntt_loop_started(ntt_loop_t* loop, void* ctx)
{
    app_t* app = app_from_ctx(ctx);
    ntt_signal_source_start(app_from_ctx(ctx)->signal_source, loop);
    printf("waiting signal...\n");
}

int main(int argc, char* argv[])
{
    ntt_signal_setup(SIGINT);
    app_t app;
    app.signal_source = ntt_signal_source_create(&g_app_signal_source_vtbl, &app, SIGINT);
    ntt_loop_svc(ntt_loop_cbs_make(ntt_loop_started), &app, 4);
    ntt_signal_source_destroy(app.signal_source);
    printf("app stopped\n");
    ntt_signal_revert(SIGTERM);
    return 0;
}
