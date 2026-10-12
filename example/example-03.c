// Read standard input through the loop: the reader holds the loop running, and
// the end of the input stops both.

#include <ntt/loop.h>
#include <ntt/queue.h>
#include <ntt/reader.h>
#include <ntt/task.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void app_on_data(
    ntt_reader_t* self,
    void* ctx,
    uint8_t* data,
    size_t size)
{
    (void)self;
    (void)ctx;

    printf("read %zu bytes: ", size);
    fwrite(data, 1, size, stdout);
    fflush(stdout);
}

static void app_on_cancel(ntt_reader_t* self, void* ctx, int ec)
{
    (void)ctx;

    if (ec != 0) {
        printf("stdin failed with %i\n", ec);
    } else {
        printf("stdin is at its end\n");
    }

    ntt_reader_delete(self);
}

static const ntt_reader_vtbl_t g_stdin_reader = {
    .name      = "stdin-reader",
    .on_data   = app_on_data,
    .on_cancel = app_on_cancel,
};

static void app_on_start(void* ctx, ntt_task_action_t action)
{
    (void)ctx;

    if (action != NTT_TASK_RUN) {
        return;
    }

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);

    if (flags == -1 || fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl");
        abort();
    }

    ntt_reader_t* reader = ntt_reader_create(&g_stdin_reader, NULL);

    if (reader == NULL) {
        abort();
    }

    ntt_reader_start(reader, STDIN_FILENO, ntt_queue_loop(ntt_queue_self()));
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    ntt_loop_run(ntt_task_create(app_on_start), 4);

    printf("app stopped\n");

    return 0;
}
