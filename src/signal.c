#include "ntt/signal.h"
#include "ntt/defs.h"

#include <signal.h>
#include <stddef.h>
#include <sys/eventfd.h>

typedef struct ntt_signal {
    struct sigaction old_action;
    int eventfd;
} ntt_signal_t;

static void ntt_signal_raise(ntt_signal_t* self)
{
    uint64_t value = 1;
    write(self->eventfd, &value, 8);
}

static ntt_signal_t g_siterm = {
    .old_action = { 0 },
    .eventfd = -1,
};

static ntt_signal_t g_sigint = {
    .old_action = { 0 },
    .eventfd = -1,
};

static ntt_signal_t g_sighup = {
    .old_action = { 0 },
    .eventfd = -1,
};

static ntt_signal_t* ntt_signal_select(int sig)
{
    switch (sig) {
    case SIGTERM:
        return &g_siterm;
    case SIGINT:
        return &g_sigint;
    case SIGHUP:
        return &g_sighup;
    default:
        return NULL;
    }
}

static void ntt_signal_handler(int sig)
{
    ntt_signal_t* signal = ntt_signal_select(sig);
    if (signal != NULL) {
        ntt_signal_raise(signal);
    }
}

void ntt_signal_setup(int sig)
{
    ntt_signal_t* signal = ntt_signal_select(sig);

    if (signal == NULL) {
        return;
    }

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, sig);

    pthread_sigmask(SIG_BLOCK, &set, NULL);

    signal->eventfd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);

    struct sigaction new_sigint_action = { 0 };
    new_sigint_action.sa_handler = ntt_signal_handler;
    sigaction(sig, &new_sigint_action, &signal->old_action);

    pthread_sigmask(SIG_UNBLOCK, &set, NULL);
}

void ntt_signal_revert(int sig)
{
    ntt_signal_t* signal = ntt_signal_select(sig);

    if (ntt_unlikely(signal == NULL)) {
        return;
    }

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, sig);

    pthread_sigmask(SIG_BLOCK, &set, NULL);

    sigaction(sig, &signal->old_action, NULL);

    close(signal->eventfd);

    pthread_sigmask(SIG_UNBLOCK, &set, NULL);
}

int ntt_signal_get_eventfd(int sig)
{
    ntt_signal_t* signal = ntt_signal_select(sig);
    return signal != NULL ? signal->eventfd : -1;
}
