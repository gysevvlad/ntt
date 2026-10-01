#include <ntt/ntt.h>

#include <stdio.h>

void app_on_start(void* ctx, ntt_task_action_t action)
{
    (void)ctx;

    if (action != NTT_TASK_RUN) {
        return;
    }

    printf("Hello from ntt_loop!\n");
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    ntt_loop_run(
        ntt_task_create(app_on_start),
        1);

    return 0;
}
