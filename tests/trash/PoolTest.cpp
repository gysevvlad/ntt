#include "ntt/pool.h"
#include "ntt/event_source.h"
#include "ntt/impl/event_source.h"
#include "ntt/ntt.hpp"
#include "ntt/reader.h"
#include "ntt/task_queue.h"

#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <future>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <mutex>
#include <ratio>
#include <span>
#include <system_error>
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

std::mutex g_cout_mutex;

static std::size_t transferred_bytes_rx = 0;

static constexpr ntt_reader_listener_tbl_t g_reader_listener_tbl {
    .on_data = [](void* ctx, ntt_reader_t* reader, uint8_t* data, size_t size) {
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
        transferred_bytes_rx += size;
        std::unique_lock lock{g_cout_mutex};
        std::cout << "got " << size << " bytes\n";
        return 0; },
    .on_stop = [](void* ctx, ntt_reader_t* reader, int ec) {
        ntt_reader_destroy(reader);
        std::unique_lock lock{g_cout_mutex};
        std::cout << "reader stopped with ec: " << ec << std::make_error_code(errc { ec }).message() << "\n";
        static_cast<std::promise<void>*>(ctx)->set_value(); },
};

struct DummyPipeWriter {
    void write(std::span<const std::uint8_t> data)
    {
        std::lock_guard lock { m };

        if (is_closing()) {
            return;
        }

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
            rc = ::write(fd, data.data() + transferred, data.size() - transferred);
        } while (rc > 0 || (rc == -1 && errno == EINTR));

        if (rc == 0) {
            return;
        }

        assert(rc == -1);

        int ec = errno;
        if (ec == EWOULDBLOCK || ec == EAGAIN) {
            buffer.resize(data.size());
            std::memcpy(buffer.data(), data.data() - transferred, data.size() - transferred);
            wait_ready = true;
            return;
        }

        auto error = std::make_error_code(errc { errno });

        std::unique_lock l { g_cout_mutex };
        std::cerr << "[debug]" << error << ' ' << error.message() << '\n';
    }

    int wakeup(ntt_event_source_t* src, int events)
    {
        {
            std::unique_lock l { g_cout_mutex };
            std::cout << "ntt_pipe_writer wakeup" << '\n';
        }

        assert(events == NTT_WRITE_EVENT);

        std::lock_guard l { m };

        if (buffer.empty()) {
            return 0;
        }

        int rc = 0;
        int transferred = 0;
        do {
            transferred += rc;
            if (remain_to_close != std::numeric_limits<std::size_t>::max()) {
                remain_to_close -= rc;
            }
            if (buffer.size() - transferred == 0) {
                if (remain_to_close == 0) {
                    ntt_event_source_cancel(event);
                }
                return 0;
            }
            rc = ::write(fd, buffer.data() + transferred, buffer.size() - transferred);
        } while (rc > 0 || (rc == -1 && errno == EINTR));

        assert(rc == -1);

        int ec = errno;

        if (ec == EWOULDBLOCK || ec == EAGAIN) {
            ::memmove(buffer.data(), buffer.data() + transferred, buffer.size() - transferred);
            buffer.resize(buffer.size() - transferred);
            if (buffer.empty()) {
                wait_ready = false;
            }
            if (remain_to_close == 0) {
                ntt_event_source_cancel(event);
            }
            return 0;
        }

        return ec;
    }

    static int wakeup_svc(ntt_event_source_t* src, void* ctx, int events)
    {
        return static_cast<DummyPipeWriter*>(ctx)->wakeup(src, events);
    }

    void stop()
    {
        std::lock_guard l { m };
        remain_to_close = buffer.size();
        if (remain_to_close == 0) {
            ntt_event_source_cancel(event);
        }
    }

    void on_del(ntt_event_source_t* src)
    {
        close(fd);
        ntt_event_source_destroy(src);
        std::lock_guard l { g_cout_mutex };
        std::cout << "writer destroyed \"";
    }

    static void on_del_svc(ntt_event_source_t* src, void* ctx)
    {
        return static_cast<DummyPipeWriter*>(ctx)->on_del(src);
    }

    [[nodiscard]] bool is_closing() const
    {
        return remain_to_close != std::numeric_limits<std::size_t>::max();
    }

    std::size_t remain_to_close = std::numeric_limits<std::size_t>::max();
    ntt_event_source_t* event;
    std::mutex m;
    bool wait_ready { false };
    int fd;
    std::vector<std::uint8_t> buffer;
};

ntt_event_handler_tbl_t g_pipe_writer_event_handler_tbl = {
    .name = "pipe_writer",
    .on_ready_cb = DummyPipeWriter::wakeup_svc,
    .on_del_cb = DummyPipeWriter::on_del_svc,
};

std::size_t transferred_bytes_tx = 0;

TEST_F(PoolTest, Pipe)
{
    static constexpr std::size_t g_cnt = 1'000;
    static constexpr std::size_t g_pool_width = 4;
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

    std::promise<void> reader_stop_promise;
    auto reader_stop_future = reader_stop_promise.get_future();

    auto* reader = ntt_reader_create(
        pool,
        fifo[0],
        &g_reader_listener_tbl,
        &reader_stop_promise);

    ntt_reader_start(reader);

    DummyPipeWriter writer;
    writer.fd = fifo[1];

    auto* write_source = ntt_event_source_create(
        pool,
        &g_pipe_writer_event_handler_tbl,
        &writer,
        fifo[1],
        NTT_WRITE_EVENT);

    writer.event = write_source;
    ntt_event_source_start(write_source);

    for (std::size_t i = 0; i < g_cnt; ++i) {
        std::array<std::uint8_t, 1024> data {};
        writer.write(data);
        transferred_bytes_tx += 1024;
        std::this_thread::sleep_for(std::chrono::microseconds { 1 });
    }

    writer.stop();

    reader_stop_future.wait();

    ntt_pool_release(pool);
    f.wait();

    std::cout << "TX: " << transferred_bytes_tx << '\n';
    std::cout << "RX: " << transferred_bytes_rx << '\n';
    ASSERT_EQ(transferred_bytes_tx, transferred_bytes_rx);
}
