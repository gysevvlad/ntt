#include "./worker.h"
#include "ntt/task.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <future>
#include <random>
#include <thread>

#include <sys/epoll.h>

namespace {

int make_epoll()
{
    int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd == -1) {
        throw std::system_error { epoll_fd, std::system_category() };
    }
    return epoll_fd;
}

} // namespace

TEST(WorkerTest, CreateAndStop)
{
    std::jthread thread {
        [&] {
            int fd = make_epoll();
            ntt_worker_svc(
                ntt_worker_cbs_t {
                    .enter_cb = [](void* ctx, ntt_worker_t* worker) {},
                    .svc_cb   = [](void* ctx, sigset_t* sigset) {
                        std::array<struct epoll_event, 1> events{};
                        epoll_pwait(*static_cast<int*>(ctx), events.data(), events.size(), -1, sigset); },
                    .leave_cb = [](void* cxt) {},
                },
                &fd);
            close(fd);
        }
    };
}

TEST(WorkerTest, PushLocalTask)
{
    struct Context {
        std::promise<ntt_worker_t*> promise;
        int fd = make_epoll();
    } context;

    bool local_task_done = false;

    {
        auto future = context.promise.get_future();
        std::jthread thread {
            [&] {
                ntt_worker_svc(
                    ntt_worker_cbs_t {
                        .enter_cb = [](void* ctx, ntt_worker_t* worker) { static_cast<Context*>(ctx)->promise.set_value(ntt_worker_acquire(worker)); },
                        .svc_cb   = [](void* ctx, sigset_t* sigset) {
                        std::array<struct epoll_event, 1> events{};
                        epoll_pwait(static_cast<Context*>(ctx)->fd, events.data(), events.size(), -1, sigset); },
                        .leave_cb = [](void* cxt) {},
                    },
                    &context);
            }
        };

        auto* worker = future.get();

        ntt_worker_push_task(worker, ntt::make_task([&] {
            local_task_done = true;
        }));

        ntt_worker_release(worker);
    }

    ASSERT_TRUE(local_task_done);

    close(context.fd);
}

TEST(WorkerTest, PushLocalTasks)
{
    static constexpr std::size_t g_task_cnt = 10'000;

    static constexpr std::size_t g_send_task_max_interval_in_us = 5;
    static constexpr std::size_t g_svc_task_max_duration_in_us  = 5;

    std::random_device rd;

    auto seed1 = rd();
    std::mt19937 gen1(seed1);
    std::uniform_int_distribution<> dist1(1, g_send_task_max_interval_in_us);
    std::cout << "seed 1: " << seed1 << '\n';

    auto seed2 = rd();
    std::mt19937 gen2(seed2);
    std::uniform_int_distribution<> dist2(1, g_svc_task_max_duration_in_us);
    std::cout << "seed 2: " << seed2 << '\n';

    struct Context {
        std::promise<ntt_worker_t*> promise;
        int fd                      = make_epoll();
        std::size_t interrupt_count = 0;
    } context;

    std::size_t cnt = 0;

    {
        auto future = context.promise.get_future();
        std::jthread thread {
            [&] {
                ntt_worker_svc(
                    ntt_worker_cbs_t {
                        .enter_cb = [](void* ctx, ntt_worker_t* worker) { static_cast<Context*>(ctx)->promise.set_value(ntt_worker_acquire(worker)); },
                        .svc_cb   = [](void* ctx, sigset_t* sigset) {
                        std::array<struct epoll_event, 1> events{};
                        epoll_pwait(static_cast<Context*>(ctx)->fd, events.data(), events.size(), -1, sigset);
                        static_cast<Context*>(ctx)->interrupt_count += 1; },
                        .leave_cb = [](void* cxt) {},
                    },
                    &context);
            }
        };

        auto* worker = future.get();

        for (std::size_t i = 0; i < g_task_cnt; ++i) {
            std::this_thread::sleep_for(std::chrono::microseconds { dist1(gen1) });
            ntt_worker_push_task(worker, ntt::make_task([&] {
                cnt += 1;
                std::this_thread::sleep_for(std::chrono::microseconds { dist2(gen2) });
            }));
        }

        ntt_worker_release(worker);
    }

    ASSERT_EQ(cnt, g_task_cnt);
    std::cout << "interrup count: " << context.interrupt_count << '\n';

    close(context.fd);
}
