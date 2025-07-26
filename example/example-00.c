#include <ntt/ntt.h>

#include <stdio.h>

void ntt_loop_started(ntt_loop_t* loop, void* ctx)
{
    printf("hello from ntt_loop!\n");
}

int main(int argc, char* argv[])
{
    ntt_loop_vptr_t loop_vtbl = {
        .name = "example-00",
        .started = ntt_loop_started,
    };
    ntt_loop_svc(&loop_vtbl, NULL, 4);
    return 0;
}
