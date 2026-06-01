#include "ntt/signal.h"
#include "./signal.h"
#include "ntt/defs.h"

#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/eventfd.h>

const char* sig_name(int sig)
{
    /* Thread-local buffer so it's safe in multithreaded code */
    static __thread char buf[32];

    if (sig >= SIGRTMIN && sig <= SIGRTMAX) {
        snprintf(buf, sizeof(buf), "SIGRTMIN+%d", sig - SIGRTMIN);
        return buf;
    }

    /* Portable fallback: limited manual mapping */
    switch (sig) {
    case SIGINT:
        return "SIGINT";
    case SIGTERM:
        return "SIGTERM";
    case SIGKILL:
        return "SIGKILL";
    case SIGSEGV:
        return "SIGSEGV";
    case SIGABRT:
        return "SIGABRT";
    case SIGPIPE:
        return "SIGPIPE";
    case SIGALRM:
        return "SIGALRM";
    case SIGHUP:
        return "SIGHUP";
    case SIGQUIT:
        return "SIGQUIT";
    case SIGUSR1:
        return "SIGUSR1";
    case SIGUSR2:
        return "SIGUSR2";
    default:
        snprintf(buf, sizeof(buf), "UNKNOWN(%d)", sig);
        return buf;
    }
}

int ntt_sigup_blocked()
{
    sigset_t current_mask;
    if (pthread_sigmask(SIG_SETMASK, NULL, &current_mask) != 0) {
        perror("pthread_sigmask");
        return 1;
    }
    return sigismember(&current_mask, NTT_SIGRTUP);
}

void ntt_print_blocked_signals(sigset_t* sigset)
{
    int i;
    for (i = 0; i < NSIG; ++i) {
        if (sigismember(sigset, i)) {
            printf("%s\n", sig_name(i));
        }
    }
}

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
    .eventfd    = -1,
};

static ntt_signal_t g_sigint = {
    .old_action = { 0 },
    .eventfd    = -1,
};

static ntt_signal_t g_sighup = {
    .old_action = { 0 },
    .eventfd    = -1,
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
    new_sigint_action.sa_handler       = ntt_signal_handler;
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
