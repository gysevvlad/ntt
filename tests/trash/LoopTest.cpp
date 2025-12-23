#include <ntt/ntt.hpp>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstring>
#include <deque>
#include <mutex>

#include <gtest/gtest.h>
#include <span>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <thread>
#include <tuple>

class LoopTest : public testing::Test {
};

TEST_F(LoopTest, NothingTodoTask)
{
    // just start/stop test

    bool started = false;
    static ntt_loop_vptr_t g_loop_vptr {
        .name = "test_loop",
        .started = []([[maybe_unused]] ntt_loop_t* loop, void* ctx) {
            *static_cast<bool*>(ctx) = true;
        }
    };
    ntt_loop_svc(&g_loop_vptr, &started, 4);
    ASSERT_TRUE(started);
}

namespace ntt {

// TODO(vgusev): split event and event handler to allow pure virtual inheritance
class epoll_event {
public:
    epoll_event(int fd, uint32_t events)
    {
        m_event.fd = fd;
        m_event.vtbl = &vtbl;
        m_event.ctx = this;
        m_event.events = events;
    }

    virtual void ready(uint32_t events) = 0;
    virtual void canceled() = 0;
    virtual ~epoll_event();

    ntt_epoll_event_t* native_handler() { return &m_event; }

private:
    static void on_ready(ntt_epoll_event_t* self, void* ctx, uint32_t events);
    static void on_cancelled(ntt_epoll_event_t* self, void* ctx);

    ntt_epoll_event_t m_event {};

    const static ntt_epoll_event_vtbl_t vtbl;
};

epoll_event::~epoll_event() = default;

void epoll_event::on_ready([[maybe_unused]] ntt_epoll_event_t* self, void* ctx, uint32_t events)
{
    // TODO(vgusev): exception handling
    static_cast<epoll_event*>(ctx)->ready(events);
}

void epoll_event::on_cancelled([[maybe_unused]] ntt_epoll_event_t* self, void* ctx)
{
    // TODO(vgusev): exception handling
    static_cast<epoll_event*>(ctx)->canceled();
}

const ntt_epoll_event_vtbl_t epoll_event::vtbl {
    .name = "ntt::epoll_event",
    .ready = epoll_event::on_ready,
    .cancelled = epoll_event::on_cancelled,
};

} // namespace ntt

namespace ntt {

class loop {
public:
    virtual void on_started() = 0;

    virtual ~loop();

    template <class Impl, class... Args>
    static void svc(Args&&... args)
    {
        Impl impl { std::forward<Args>(args)... };
        ntt_loop_svc(&vtbl, static_cast<loop*>(&impl), 4);
    }

    void add(epoll_event& epoll_event)
    {
        ntt_loop_add_epoll_event(m_loop, epoll_event.native_handler());
    }

    void del(epoll_event& epoll_event)
    {
        ntt_loop_del_epoll_event(m_loop, epoll_event.native_handler());
    }

private:
    ntt_loop_t* m_loop { nullptr };

    static void on_started(ntt_loop_t* l, void* ctx);
    const static ntt_loop_vptr_t vtbl;
};

loop::~loop() = default;

void loop::on_started(ntt_loop_t* l, void* ctx)
{
    auto& self = *static_cast<loop*>(ctx);
    self.m_loop = l;
    try {
        self.on_started();
    } catch (...) {
        // TODO(vgusev): exception handling
        abort();
    }
}

const ntt_loop_vptr_t loop::vtbl {
    .name = "ntt::loop",
    .started = loop::on_started,
};

} // namespace ntt

TEST_F(LoopTest, NttLoop)
{
    class loop : public ntt::loop {
    public:
        explicit loop(bool* started)
            : m_started { started }
        {
        }

        void on_started() override
        {
            *m_started = true;
        }

        ~loop() override = default;

    private:
        bool* m_started;
    };

    bool started = false;

    ntt::loop::svc<loop>(&started);
}

TEST_F(LoopTest, Wakeup)
{
    // oneshot eventfd wakeup
    static constexpr auto g_send_delay = std::chrono::milliseconds { 100 };

    int event_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE);
    ASSERT_NE(event_fd, -1);

    std::thread sender { [event_fd] {
        std::this_thread::sleep_for(g_send_delay);
        int rc = eventfd_write(event_fd, 1);
        if (rc != 0) {
            // TODO(vgusev): impl channel to ntt loop
            // we can't notify ntt loop about unexpected condition, so just stop execution
            int ec = errno;
            std::cerr << "eventfd_write failed with: " << ec
                      << "\n";
            abort();
        }
    } };

    class loop
        : public ntt::loop,
          public ntt::epoll_event {
    public:
        loop(bool* started, bool* wakeup, bool* canceled, int event_fd)
            : epoll_event { event_fd, EPOLLIN | EPOLLET }
            , m_started { started }
            , m_wakeup { wakeup }
            , m_canceled { canceled }
        {
        }

        void on_started() override
        {
            *m_started = true;
            add(*this);
        }

        void ready([[maybe_unused]] uint32_t events) override
        {
            *m_wakeup = true;
            del(*this);
        }

        void canceled() override
        {
            *m_canceled = true;
        }

        ~loop() override = default;

    private:
        bool* m_started;
        bool* m_wakeup;
        bool* m_canceled;
    };

    bool started = false;
    bool wakeup = false;
    bool canceled = false;

    ntt::loop::svc<loop>(&started, &wakeup, &canceled, event_fd);

    sender.join();

    close(event_fd);

    ASSERT_TRUE(started);
    ASSERT_TRUE(wakeup);
    ASSERT_TRUE(canceled);
};

TEST_F(LoopTest, SignalCatch)
{
    // stop ntt_loop via signal

    static constexpr auto g_send_delay = std::chrono::milliseconds { 10 };

    ntt_signal_setup(SIGTERM);

    std::thread sender { [] {
        for (std::size_t i = 0; i < 2; ++i) {
            std::this_thread::sleep_for(g_send_delay);
            if (std::raise(SIGTERM) != 0) {
                std::cerr << "failed to raise SIGTERM\n";
                std::abort();
            }
        }
    } };

    class loop
        : public ntt::loop,
          public ntt::epoll_event {
    public:
        explicit loop()
            : epoll_event { ntt_signal_get_eventfd(SIGTERM), EPOLLIN | EPOLLET }
        {
        }

        void on_started() override
        {
            add(*this);
        }

        bool lock_input()
        {
            auto old_state = m_state.load(std::memory_order_relaxed);

            for (;;) {
                if (ntt_unlikely(old_state.lock_read)) {
                    // other thread is processing INPUT

                    // set WANT_INPUT bit
                    auto new_state = old_state;
                    new_state.want_read = true;

                    if (m_state.compare_exchange_strong(old_state, new_state)) {
                        // other thread will do the reading again
                        return false;
                    }
                } else {
                    // current thread want to process INPUT

                    assert(!old_state.want_read);

                    // set INPUT lock bit
                    auto new_state = old_state;
                    new_state.lock_read = true;

                    if (m_state.compare_exchange_strong(old_state, new_state)) {
                        // current thread take input lock
                        return true;
                    }
                }

                // try again
            };
        }

        bool unlock_input()
        {
            auto old_state = m_state.load(std::memory_order_relaxed);

            for (;;) {
                if (ntt_unlikely(old_state.want_read)) {
                    // other thread got read notification

                    assert(old_state.lock_read);

                    // reset WANT_READ bit
                    auto new_state = old_state;
                    new_state.want_read = false;

                    if (m_state.compare_exchange_strong(old_state, new_state)) {
                        // current thread should read fd again
                        return false;
                    }
                } else {
                    // nothing to read

                    // reset LOCK_READ bit
                    auto new_state = old_state;
                    new_state.lock_read = false;

                    if (m_state.compare_exchange_strong(old_state, new_state)) {
                        // current thread take release the lock
                        return true;
                    }
                }

                // try again
            };
        }

        void do_read()
        {
            eventfd_t value = 0;
            int rc = 0;
            int ec = 0;
            do {
                rc = eventfd_read(native_handler()->fd, &value);
                if (rc != 0) {
                    ec = errno;
                } else {
                    m_cnt += value;
                    if (m_cnt == 2) {
                        del(*this);
                    }
                }
            } while ((rc == 0) || (ec == EINTR));
            std::this_thread::sleep_for(std::chrono::milliseconds { 15 });
        }

        void ready([[maybe_unused]] uint32_t events) override
        {
            if ((events & EPOLLIN) != 0) {
                if (lock_input()) {
                    do {
                        do_read();
                    } while (!unlock_input());
                } else {
                    // are busy
                    std::cerr << "busy...\n";
                }
            }
        }

        void canceled() override
        {
        }

        ~loop() override = default;

    private:
        struct State {
            bool lock_read : 1 = false;
            bool want_read : 1 = false;
        };
        std::atomic<State> m_state;
        static_assert(std::atomic<State>::is_always_lock_free);
        std::atomic<std::size_t> m_cnt { 0 };
    };

    ntt::loop::svc<loop>();

    sender.join();

    ntt_signal_revert(SIGTERM);
};
