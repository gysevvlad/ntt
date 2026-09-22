#include <ntt/ntt.h>

#include <signal.h>
#include <stdio.h>

struct app {
    int stopped;
    ntt_loop_t* loop;
};
typedef struct app app_t;

void app_init(app_t* self)
{
    self->stopped = 0;
    self->loop    = NULL;
}

static void app_on_signal(ntt_loop_t* loop, void* ctx, int signal)
{
    switch (signal) {
    case SIGINT:
        printf("got SIGINT!\n");
        break;

    case SIGTERM:
        printf("got SIGTERM!\n");
        break;

    case SIGHUP:
        printf("got SIGHUP!\n");
        break;

    default:
        printf("got signal with number %i!\n", signal);
        break;
    }

    app_t* self = ctx;

    if (self->stopped == 0) {
        printf("stopping...\n");
        self->stopped = 1;
        ntt_loop_release(loop);
        return;
    }
}

void app_on_start(ntt_loop_t* loop, void* ctx)
{
    app_t* app = ctx;
    app->loop  = ntt_loop_acquire(loop);

    printf("waiting signal...\n");
}

int main(int argc, char* argv[])
{
    app_t app;
    app_init(&app);

    ntt_loop_svc(
        ntt_loop_cbs_make(
            app_on_start,
            app_on_signal),
        &app,
        1);

    printf("app stopped\n");
    return 0;
}
