#include "ntt/log.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>

pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

static int g_ntt_log_enabled = 0;

__attribute__((constructor)) void mylibrary_load(void)
{
    if (getenv("NTT_ENABLE_LOG")) {
        g_ntt_log_enabled = 1;
    }
}

static void ntt_log_iml(const char* fmt, va_list args)
{
    if (!g_ntt_log_enabled) {
        return;
    }
    pthread_mutex_lock(&g_mutex);
    vprintf(fmt, args);
    pthread_mutex_unlock(&g_mutex);
}

void ntt_log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    ntt_log_iml(fmt, args);
    va_end(args);
}
