#include <ntt/ntt.h>

#include <stdio.h>

void app_on_start(ntt_loop_t* loop, void* ctx)
{
    printf("hello from ntt_loop!\n");
}

void ntt_got_signal(ntt_loop_t* loop, void* ctx, int signal)
{
    printf("got signal!\n");
}

int main(int argc, char* argv[])
{
    ntt_loop_svc(ntt_loop_cbs_make(app_on_start, ntt_got_signal), NULL, 4);
    return 0;
}
