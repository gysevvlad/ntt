#include "ntt/impl/thread.h"
#include "ntt/ntt.hpp"

#include <gtest/gtest.h>
#include <mutex>
#include <pthread.h>
#include <sys/epoll.h>

#include <future>
#include <thread>

class ThreadTest : public testing::Test {
    void SetUp() override { m_epoll_fd = epoll_create1(EPOLL_CLOEXEC); }

    void TearDown() override { close(m_epoll_fd); }

protected:
    int get_epoll_fd() { return m_epoll_fd; }

private:
    int m_epoll_fd;
};

TEST_F(ThreadTest, Example)
{
    std::promise<void> p;
    auto f = p.get_future();
    ntt_thread_t thread;
    ntt_thread_init(
        &thread,
        +[](ntt_thread_t* thread, void* context) {
            static_cast<std::promise<void>*>(context)->set_value();
        },
        &p, get_epoll_fd());
    ntt_thread_send_stop(&thread);
    f.wait();
    pthread_join(thread.id, NULL);
}

TEST_F(ThreadTest, Post)
{
    std::promise<void> p;
    auto f = p.get_future();
    ntt_thread_t thread;
    ntt_thread_init(
        &thread,
        +[](ntt_thread_t* thread, void* context) {
            static_cast<std::promise<void>*>(context)->set_value();
        },
        &p, get_epoll_fd());
    bool task_done = false;
    ntt_thread_send_task(&thread,
        ntt::make_task([&task_done] { task_done = true; }));
    ntt_thread_send_stop(&thread);
    f.wait();
    pthread_join(thread.id, NULL);
    ASSERT_TRUE(task_done);
}

TEST_F(ThreadTest, PostChain)
{
    std::promise<void> p;
    auto f = p.get_future();
    ntt_thread_t thread;
    ntt_thread_init(
        &thread,
        +[](ntt_thread_t* thread, void* context) {
            static_cast<std::promise<void>*>(context)->set_value();
        },
        &p, get_epoll_fd());
    std::promise<void> p2;
    auto f2 = p2.get_future();
    std::promise<void> p3;
    auto f3 = p3.get_future();
    ntt_thread_send_task(&thread, ntt::make_task([&p2, &f3] {
        p2.set_value();
        f3.wait();
    }));
    f2.wait();
    int c = 0;
    for (std::size_t i = 0; i < 128; ++i) {
        ntt_thread_send_task(&thread, ntt::make_task([&c] { ++c; }));
    }
    ntt_thread_send_stop(&thread);
    p3.set_value();
    f.wait();
    pthread_join(thread.id, NULL);
    ASSERT_EQ(c, 128);
}

TEST_F(ThreadTest, SendTask)
{
    static constexpr std::size_t g_cnt = 1'000'000;
    ntt_thread thread;
    std::promise<void> p;
    auto f = p.get_future();
    ntt_thread_init(
        &thread, [](auto, void* ctx) { static_cast<std::promise<void>*>(ctx)->set_value(); },
        &p,
        get_epoll_fd());
    std::size_t cnt = 0;
    for (std::size_t i = 0; i < g_cnt; ++i) {
        ntt_thread_send_task(&thread, ntt::make_task([&cnt] { cnt += 1; }));
    }
    ntt_thread_send_stop(&thread);
    f.wait();
    ASSERT_EQ(cnt, g_cnt);
};
