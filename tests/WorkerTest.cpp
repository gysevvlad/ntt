#include "ntt/worker.h"
#include "ntt/sigset.h"
#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <ntt/ntt.hpp>
#include <thread>
#include <utility>

class TestWorker {
public:
    const static ntt_worker_cbs_t g_cbs;

private:
    void enter(ntt_worker_t* worker)
    {
        m_worker = worker;
        std::cerr << "worker enter" << '\n';
    }

    void leave()
    {
        std::cerr << "worker leave" << '\n';
    }

    int svc(ntt_sigset_t* sigset)
    {
        return -1;
    }

    void err(int ec)
    {
        std::cerr << "got err " << ec << '\n';
        ntt_worker_release(m_worker);
        m_worker = nullptr;
    }

    std::condition_variable cv;
    std::mutex m;
    ntt_worker_t* m_worker = nullptr;
};

constexpr ntt_worker_cbs_t TestWorker::g_cbs = {
    .enter_cb = [](void* ctx, ntt_worker_t* worker) { return static_cast<TestWorker*>(ctx)->enter(worker); },
    .leave_cb = [](void* ctx) { static_cast<TestWorker*>(ctx)->leave(); },
    .svc_cb = [](void* ctx, ntt_sigset_t* sigset) { return static_cast<TestWorker*>(ctx)->svc(sigset); },
    .err_cb = [](void* ctx, int ec) { return static_cast<TestWorker*>(ctx)->err(ec); },
};

// TODO(vgusev): move to ntt/ntt.hpp
namespace ntt {

struct ntt_acquire_tag_t { };
static constexpr ntt_acquire_tag_t ntt_acquire_tag;

class worker {
public:
    worker() = default;

    explicit worker(ntt_worker_t* worker)
        : m_worker { worker }
    {
    }

    worker(ntt_worker_t* worker, ntt_acquire_tag_t)
        : m_worker { worker != nullptr ? ntt_worker_acquire(worker) : nullptr }
    {
    }

    worker(const worker& other)
        : m_worker { other.m_worker != nullptr ? ntt_worker_acquire(other.m_worker) : nullptr }
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
        if (m_worker != nullptr) {
            ntt_worker_release(m_worker);
        }
        if (other.m_worker != nullptr) {
            ntt_worker_acquire(other.m_worker);
        }
        m_worker = other.m_worker;
        return *this;
    }

    worker& operator=(worker&& other) noexcept
    {
        if (m_worker != nullptr) {
            ntt_worker_release(m_worker);
        }
        m_worker = std::exchange(other.m_worker, nullptr);
        return *this;
    }

    ~worker()
    {
        if (m_worker != nullptr) {
            ntt_worker_release(m_worker);
        }
    }

    template <class F>
    void post(F&& f)
    {
        assert(m_worker != nullptr);
        ntt_worker_post_task(m_worker, make_task(std::forward<F>(f)));
    }

private:
    ntt_worker_t* m_worker = nullptr;
};

// TODO: move to cpp
constexpr ntt_worker_cbs_t g_spawn_cbs = {
    .enter_cb = [](void* ctx, ntt_worker_t* worker) { return static_cast<std::promise<ntt_worker_t*>*>(ctx)->set_value(worker); },
    .leave_cb = nullptr,
    .svc_cb = nullptr,
    .err_cb = nullptr,
};

std::pair<std::jthread, ntt::worker> spawn_worker()
{
    std::promise<ntt_worker_t*> p;
    auto f = p.get_future();
    std::jthread thread {
        [p = std::move(p)] mutable {
            ntt_worker_svc(&g_spawn_cbs, &p);
        }
    };
    return { std::move(thread), ntt::worker { f.get() } };
}

} // namespace ntt

TEST(NttWorkerTest, Post)
{
    static constexpr std::size_t g_expected_cnt = 1'000'000;
    std::size_t cnt = 0;
    {
        auto [_, worker] = ntt::spawn_worker();
        for (std::size_t i = 0; i < g_expected_cnt; ++i) {
            worker.post([&cnt] { cnt += 1; });
        }
    }
    ASSERT_EQ(cnt, g_expected_cnt);
}
