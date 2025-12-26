#include "ntt/worker.h"
#include <bits/types/sigset_t.h>
#include <chrono>
#include <ntt/ntt.hpp>

#include <gtest/gtest.h>

#include <csignal>
#include <thread>
#include <utility>

#include <pthread.h>
#include <sys/epoll.h>

namespace ntt {

class worker {
public:
    worker() = default;

    explicit worker(ntt_worker_t* worker)
        : m_worker { ntt_worker_acquire(worker) }
    {
    }

    worker(const worker& other)
        : m_worker { ntt_worker_acquire(other.m_worker) }
    {
    }

    worker(worker&& other) noexcept
        : m_worker { std::exchange(other.m_worker, nullptr) }
    {
    }

    worker& operator=(const worker& other)
    {
        if (this == &other) {
            return *this;
        }
        ntt_worker_release(m_worker);
        m_worker = ntt_worker_acquire(other.m_worker);
        return *this;
    }

    worker& operator=(worker&& other) noexcept
    {
        ntt_worker_release(m_worker);
        m_worker = std::exchange(other.m_worker, nullptr);
        return *this;
    }

    ~worker()
    {
        ntt_worker_release(m_worker);
    }

    template <class F>
    void push(F&& f)
    {
        assert(m_worker != nullptr);
        ntt_worker_push_task(m_worker, make_task(std::forward<F>(f)));
    }

    template <class F>
    void push_no_wakeup(F&& f)
    {
        assert(m_worker != nullptr);
        ntt_worker_push_task_no_wakeup(m_worker, make_task(std::forward<F>(f)));
    }

    template <class F>
    void push_defer_wakeup(F&& f)
    {
        assert(m_worker != nullptr);
        ntt_worker_push_task_defer_wakeup(m_worker, make_task(std::forward<F>(f)));
    }

    void defer_wakeup()
    {
        assert(m_worker != nullptr);
        ntt_worker_defer_wakeup(m_worker, self().m_worker);
    }

    void wakeup()
    {
        assert(m_worker != nullptr);
        ntt_worker_wakeup(m_worker);
    }

    static ntt::worker self()
    {
        return worker { ntt_worker_self() };
    }

    static std::pair<std::jthread, ntt::worker> spawn()
    {
        struct Context {
            std::promise<ntt::worker> enter_promise;
        };
        auto context      = std::make_unique<Context>();
        auto enter_future = context->enter_promise.get_future();
        std::jthread thread {
            [context = std::move(context)] mutable {
                ntt_worker_svc(
                    ntt_worker_cbs_t {
                        .enter_cb = [](void* ctx, ntt_worker* worker) { static_cast<Context*>(ctx)->enter_promise.set_value(ntt::worker { worker }); },
                        .svc_cb   = []([[maybe_unused]] void* ctx, ntt_sigset_t* sigset) { sigsuspend((sigset_t*)sigset); },
                        .leave_cb = [](void* ctx) { auto context = std::unique_ptr<Context> { static_cast<Context*>(ctx) }; },
                    },
                    context.release());
            }
        };
        return { std::move(thread), enter_future.get() };
    }

private:
    ntt_worker_t* m_worker = nullptr;
};

} // namespace ntt

TEST(NttWorkerTest, SimpleRunWithEpoll)
{
    struct Context {
        bool enter_called = false;
        bool svc_called   = false;
        bool leave_called = false;
        int epoll         = epoll_create1(EPOLL_CLOEXEC);
    } context;

    ntt_worker_svc(
        ntt_worker_cbs_t {
            .enter_cb = [](void* ctx, [[maybe_unused]] ntt_worker* worker) { static_cast<Context*>(ctx)->enter_called = true; },
            .svc_cb   = [](void* ctx, [[maybe_unused]] ntt_sigset_t* sigset) { 
                auto& self = *static_cast<Context*>(ctx);
                self.svc_called = true;
                struct epoll_event event;
                int rc = epoll_pwait(self.epoll, &event, 1, 1000, (sigset_t*)sigset); },
            .leave_cb = [](void* ctx) { static_cast<Context*>(ctx)->leave_called = true; },
        },
        &context);

    EXPECT_TRUE(context.enter_called);
    EXPECT_TRUE(context.svc_called);
    EXPECT_TRUE(context.leave_called);
}

TEST(NttWorkerTest, SimpleRunWithSigmask)
{
    struct Context {
        bool enter_called = false;
        bool svc_called   = false;
        bool leave_called = false;
    } context;

    ntt_worker_svc(
        ntt_worker_cbs_t {
            .enter_cb = [](void* ctx, [[maybe_unused]] ntt_worker* worker) { static_cast<Context*>(ctx)->enter_called = true; },
            .svc_cb   = [](void* ctx, [[maybe_unused]] ntt_sigset_t* sigset) { 
                auto &self = *static_cast<Context*>(ctx);
                self.svc_called = true;
                sigset_t old;
                sigemptyset(&old);
                pthread_sigmask(SIG_SETMASK, (sigset_t*)sigset, &old);
                pthread_sigmask(SIG_SETMASK, &old, NULL); },
            .leave_cb = [](void* ctx) { static_cast<Context*>(ctx)->leave_called = true; },
        },
        &context);

    EXPECT_TRUE(context.enter_called);
    EXPECT_TRUE(context.svc_called);
    EXPECT_TRUE(context.leave_called);
}

TEST(NttWorkerTest, SimpleRunWithSigwait)
{
    struct Context {
        bool enter_called = false;
        bool svc_called   = false;
        bool leave_called = false;
    } context;

    ntt_worker_svc(
        ntt_worker_cbs_t {
            .enter_cb = [](void* ctx, [[maybe_unused]] ntt_worker* worker) { static_cast<Context*>(ctx)->enter_called = true; },
            .svc_cb   = [](void* ctx, [[maybe_unused]] ntt_sigset_t* sigset) { 
                auto &self = *static_cast<Context*>(ctx);
                self.svc_called = true;
                sigsuspend((sigset_t*)sigset); },
            .leave_cb = [](void* ctx) { static_cast<Context*>(ctx)->leave_called = true; },
        },
        &context);

    EXPECT_TRUE(context.enter_called);
    EXPECT_TRUE(context.svc_called);
    EXPECT_TRUE(context.leave_called);
}

TEST(NttWorkerTest, SimpleRunAcquireRelease)
{
    struct Context {
        bool enter_called    = false;
        bool svc_called      = false;
        bool leave_called    = false;
        ntt_worker_t* worker = nullptr;
    } context;

    ntt_worker_svc(
        ntt_worker_cbs_t {
            .enter_cb = [](void* ctx, [[maybe_unused]] ntt_worker* worker) {
                auto &self = *static_cast<Context*>(ctx);
                self.worker = ntt_worker_acquire(worker);
                self.enter_called = true; },
            .svc_cb   = [](void* ctx, [[maybe_unused]] ntt_sigset_t* sigset) { 
                auto &self = *static_cast<Context*>(ctx);
                ntt_worker_release(self.worker);
                self.worker = nullptr;
                self.svc_called = true;
                sigset_t  old;
                sigemptyset(&old);
                pthread_sigmask(SIG_SETMASK, (sigset_t*)sigset, &old);
                pthread_sigmask(SIG_SETMASK, &old, NULL); },
            .leave_cb = [](void* ctx) { static_cast<Context*>(ctx)->leave_called = true; },
        },
        &context);

    EXPECT_TRUE(context.enter_called);
    EXPECT_TRUE(context.svc_called);
    EXPECT_TRUE(context.leave_called);
}

TEST(NttWorkerTest, Spawn)
{
    ntt::worker::spawn();
}

TEST(NttWorkerTest, Send)
{
    // static constexpr std::size_t g_expected_cnt = 1'000'000;
    static constexpr std::size_t g_expected_cnt = 8;

    std::size_t cnt = 0;
    {
        auto [_, worker] = ntt::worker::spawn();
        for (std::size_t i = 0; i < g_expected_cnt; ++i) {
            worker.push([&cnt] { cnt += 1; });
        }
    }
    ASSERT_EQ(cnt, g_expected_cnt);
}

TEST(NttWorkerTest, DISABLED_PostWakeup)
{
    // static constexpr std::size_t g_expected_cnt = 1'000'000;
    static constexpr std::size_t g_expected_cnt = 8;

    std::size_t cnt = 0;
    {
        auto [_, worker] = ntt::worker::spawn();
        for (std::size_t i = 0; i < g_expected_cnt; ++i) {
            worker.push_no_wakeup([&cnt] { cnt += 1; });
        }
        worker.wakeup();
    }
    ASSERT_EQ(cnt, g_expected_cnt);
}

TEST(NttWorkerTest, PostFromOneWorkerToOther)
{
    // static constexpr std::size_t g_expected_cnt = 1'000'000;
    static constexpr std::size_t g_expected_cnt = 8;

    std::size_t cnt = 0;
    {
        auto [_1, first_worker]  = ntt::worker::spawn();
        auto [_2, second_worker] = ntt::worker::spawn();

        std::promise<void> promise;
        auto future = promise.get_future();

        first_worker.push([second_worker, &cnt, &promise] mutable {
            for (std::size_t i = 0; i < g_expected_cnt; ++i) {
                second_worker.push_no_wakeup([&cnt] mutable {
                    cnt += 1;
                });
            }
            second_worker.push_no_wakeup([&promise] {
                promise.set_value();
            });
            second_worker.defer_wakeup();
        });

        future.get();
    }
    ASSERT_EQ(cnt, g_expected_cnt);
}
