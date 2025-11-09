#include "ntt/selector.h"
#include "ntt/ntt.hpp"
#include "ntt/sigset.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <deque>
#include <gtest/gtest.h>
#include <thread>
#include <unistd.h>

class SelectorTest : public testing::Test {
private:
    void SetUp() override
    {
        // override SIGUSR1 default action
        struct sigaction action = { 0 };
        action.sa_flags = SA_SIGINFO;
        action.sa_sigaction = signal_handler;
        int rc = sigaction(SIGUSR1, &action, &m_restore_action);
        ASSERT_NE(rc, -1);

        // block SIGUSR1
        sigset_t sigset;
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGUSR1);
        sigset_t old_sigset;
        pthread_sigmask(SIG_BLOCK, &sigset, &old_sigset);
        ::memcpy(m_sigset.data, &old_sigset, sizeof(ntt_sigset_t));
    }

    void TearDown() override
    {
        // unblock SIGUSR1
        pthread_sigmask(SIG_UNBLOCK, reinterpret_cast<sigset_t*>(&m_sigset.data), nullptr);

        // return back default SIGUSR1 action
        sigaction(SIGUSR1, &m_restore_action, nullptr);
    }

protected:
    ntt_sigset_t* get_sigset() { return &m_sigset; }

    static void raise_signal()
    {
        kill(getpid(), SIGUSR1);
    }

    static bool is_stopped()
    {
        return m_signal_raised;
    }

private:
    static void signal_handler(int signo, siginfo_t* info, void* context)
    {
        m_signal_raised = true;
    }

    static std::atomic_bool m_signal_raised;

    struct sigaction m_restore_action;
    ntt_sigset_t m_sigset;
};

std::atomic_bool SelectorTest::m_signal_raised = false;

TEST_F(SelectorTest, Example)
{
    ntt_ec_t error;
    auto* selector = ntt_selector_create(&error);
    ASSERT_NE(selector, nullptr);

    std::jthread thread([] {
        std::this_thread::sleep_for(std::chrono::milliseconds { 2 });
        raise_signal();
    });

    while (!is_stopped()) {
        ntt_selector_run(selector, get_sigset());
    }

    ntt_selector_destroy(selector);
}
