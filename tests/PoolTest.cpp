#include "ntt/pool.h"
#include "ntt/event_source.h"
#include "ntt/impl/event_source.h"
#include "ntt/ntt.hpp"
#include "ntt/task_queue.h"

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <future>
#include <gtest/gtest.h>
#include <memory>
#include <mutex>
#include <span>
#include <thread>

#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>

class PoolTest : public testing::Test {
};

TEST_F(PoolTest, CreateDestroy)
{
    static constexpr std::size_t g_pool_width = 8;
    std::promise<void> p;
    auto f = p.get_future();
    ntt_pool_t* pool = ntt_pool_with_stopped_cb(
        g_pool_width,
        +[](void* ctx) {
            static_cast<std::promise<void>*>(ctx)->set_value();
        },
        &p);
    ntt_pool_release(pool);
    f.wait();
}

struct PipeReader {
    std::promise<void> p;
    std::future<void> f = p.get_future();
    std::mutex mutex;
};

ntt_event_handler_tbl_t g_pipe_reader_event_handler {
    .name = "pipe_reader",
    .on_ready_cb = +[](ntt_event_source_t* src, void* ctx, int events) { 

        std::unique_lock lock{static_cast<PipeReader*>(ctx)->mutex, std::try_to_lock};
        assert(lock.owns_lock());

        assert(events == NTT_READ_EVENT);

        std::array<std::uint8_t, 32> data;
        int rc = 0;
        int transferred = 0;
        do {
            transferred += rc;
            rc = read(src->fd, data.data(), data.size());
        } while (rc > 0 || (rc == -1 && errno == EINTR));

        std::cout << "transferred: " << transferred << '\n';

        if (rc == -1) {
            int ec = errno;
            if (ec == EWOULDBLOCK || ec == EAGAIN) {
                return 0;
            }
            return ec;
        }

        if (rc == 0) {
            std::cout << "done: " << transferred << '\n';
            ntt_event_source_cancel(src);
            return 0;
        }

        return 0; },
    .on_del_cb = +[](ntt_event_source_t* src, void* ctx) { 
        static_cast<PipeReader*>(ctx)->p.set_value();
        std::cout << "stopped with " << src->ec << '\n';
        ntt_event_source_destroy(src); }
};

struct DummyPipeWriter {
    void write(std::span<const std::uint8_t> data)
    {
        std::lock_guard lock { m };

        if (wait_ready) {
            auto old_len = buffer.size();
            buffer.resize(buffer.size() + data.size());
            std::memcpy(buffer.data() + old_len, data.data(), data.size());
            return;
        }

        int rc = 0;
        int transferred = 0;
        do {
            transferred += rc;
            rc = ::write(fd, data.data(), data.size());
        } while (rc > 0 || (rc == -1 && errno == EINTR));

        if (rc == -1) {
            int ec = errno;
            if (ec == EWOULDBLOCK || ec == EAGAIN) {
                buffer.resize(data.size());
                std::memcpy(buffer.data(), data.data() - transferred, data.size() - transferred);
                wait_ready = true;
                return;
            }

            std::cerr << std::make_error_code(errc { errno }) << '\n';
            return;
        }
    }

    std::mutex m;
    bool wait_ready;
    int fd;
    std::vector<std::uint8_t> buffer;
};

ntt_event_handler_tbl_t g_pipe_writer_event_handler_tbl = {

};

TEST_F(PoolTest, Pipe)
{
    static constexpr std::size_t g_cnt = 1'000;
    static constexpr std::size_t g_pool_width = 8;
    std::promise<void> p;
    auto f = p.get_future();
    ntt_pool_t* pool = ntt_pool_with_stopped_cb(
        g_pool_width,
        +[](void* ctx) {
            static_cast<std::promise<void>*>(ctx)->set_value();
        },
        &p);

    int fifo[2];
    ASSERT_EQ(pipe2(fifo, O_CLOEXEC | O_NONBLOCK), 0);

    PipeReader reader;

    auto source = ntt_event_source_create(pool, &g_pipe_reader_event_handler, &reader, fifo[0], NTT_READ_EVENT);
    ntt_event_source_start(source);

    for (std::size_t i = 0; i < g_cnt; ++i) {
        std::array<char, 16> data {};
        write(fifo[1], data.data(), data.size());
        std::this_thread::sleep_for(std::chrono::microseconds { 1 });
    }

    close(fifo[1]);

    reader.f.wait();

    ntt_pool_release(pool);
    f.wait();
}