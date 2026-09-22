#include <ntt/ntt.h>

#include <stdio.h>

void app_on_start(void* ctx)
{
    (void)ctx;
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
