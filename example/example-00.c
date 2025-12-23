#include <ntt/ntt.h>

#include <stdio.h>

void ntt_loop_started(ntt_loop_t* loop, void* ctx)
{
    printf("hello from ntt_loop!\n");
}

int main(int argc, char* argv[])
{
    ntt_loop_svc(ntt_loop_cbs_make(ntt_loop_started), NULL, 4);
    return 0;
}
