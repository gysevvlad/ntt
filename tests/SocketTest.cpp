#include "ntt/socket.h"
#include "ntt/loop.h"
#include "ntt/sockaddr.h"
#include <boost/asio/io_context.hpp>
#include <ntt/ntt.hpp>

#include <boost/asio.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <thread>
#include <utility>

class SocketTest : public testing::Test { };

static constexpr auto g_accept_timeout
    = std::chrono::seconds { 5 };

namespace ntt {

class loop {
public:
    virtual ~loop();

    template <class Impl, class... Args>
    static void svc(Args&&... args)
    {
        Impl impl { std::forward<Args>(args)... };
        ntt_loop_svc(&vtbl, static_cast<loop*>(&impl), 4);
    }

private:
    virtual void on_started() = 0;

protected:
    ntt_loop_t* native_handle() { return m_loop; }

private:
    ntt_loop_t* m_loop { nullptr };

    static void on_started(ntt_loop_t* l, void* ctx);
    const static ntt_loop_vptr_t vtbl;
};

loop::~loop() = default;

void loop::on_started(ntt_loop_t* loop, void* ctx)
{
    auto& self  = *static_cast<class loop*>(ctx);
    self.m_loop = loop;
    try {
        self.on_started();
    } catch (...) {
        // TODO(vgusev): exception handling
        abort();
    }
}

const ntt_loop_vptr_t loop::vtbl {
    .name    = "ntt::loop",
    .started = loop::on_started,
};

} // namespace ntt

TEST_F(SocketTest, BaseTest)
{
    static auto g_requested_accept_endpoint
        = boost::asio::ip::tcp::endpoint {
              boost::asio::ip::address_v4::loopback(),
              0
          };

    boost::asio::io_context context;

    boost::asio::ip::tcp::acceptor acceptor {
        context,
        g_requested_accept_endpoint
    };

    auto actual_accept_endpoint
        = acceptor.local_endpoint();

    GTEST_LOG_(INFO) << "listen " << actual_accept_endpoint;

    auto socket_future = std::async(
        [&acceptor] {
            return acceptor.accept();
        });

    class app : public ntt::loop {
    public:
        // TODO(vg): fix memory leak
        explicit app(const boost::asio::ip::tcp::endpoint& endpoint)
            : m_socket { nullptr }
        {
        }

    private:
        void on_started() override
        {
            ntt_socket_connect(m_socket, native_handle());
        }

        ntt_socket_t* m_socket;
    };

    ntt::loop::svc<app>(actual_accept_endpoint);

    auto socket = socket_future.get();
    GTEST_LOG_(INFO) << "accepted " << socket.remote_endpoint();
}

TEST_F(SocketTest, CheckAccept)
{
    boost::asio::io_context context;
    boost::asio::ip::tcp::socket sock{context};
    boost::asio::ip::tcp::endpoint endpoint;
    sock.connect(endpoint);
}
