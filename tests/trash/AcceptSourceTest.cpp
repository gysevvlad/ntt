#include "ntt/accept_source.h"
#include "ntt/ntt.hpp"
#include "ntt/sockaddr.h"
#include "ntt/tcp_source.h"
#include "ntt/view.h"

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <chrono>
#include <gtest/gtest.h>

#include <future>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <system_error>
#include <thread>

class AcceptSourceTest : public ::testing::Test { };

TEST_F(AcceptSourceTest, Common)
{
    static constexpr std::size_t g_connect_thread_count = 8;
    // static constexpr std::size_t g_connect_thread_count = 1;
    static constexpr std::size_t g_connect_action_count = 1024 * 16;
    // static constexpr std::size_t g_connect_action_count = 2;

    auto before = ntt_accept_source_get_wakeups_count();

    std::promise<void> p;
    auto f = p.get_future();

    auto pool = ntt_pool_with_stopped_cb(
        2,
        +[](void* ctx) {
            static_cast<std::promise<void>*>(ctx)->set_value();
        },
        &p);
    ASSERT_NE(pool, nullptr);

    // std::this_thread::sleep_for(std::chrono::milliseconds { 100 });

    auto sockaddr = ntt_sockaddr_make_from_view(ntt_view_from_literal("127.0.0.1:5656"));
    ASSERT_NE(sockaddr, nullptr);

    struct AcceptContext {
        std::promise<void> promise;
        std::size_t cnt = 0;
    } ac;

    auto future = ac.promise.get_future();

    std::promise<void> close_promise;
    auto close_future = close_promise.get_future();

    auto accept_source = ntt_accept_source_create(pool, sockaddr);

    ntt_accept_source_start(
        accept_source,
        +[](void* ctx, int fd) {
            close(fd);
            auto& ac = *static_cast<AcceptContext*>(ctx);
            ac.cnt += 1;
            if (ac.cnt == g_connect_thread_count * g_connect_action_count) {
                ac.promise.set_value();
            }
        },
        &ac,
        +[](int ec, void* ctx) {
            auto error = std::make_error_code(static_cast<std::errc>(ec));
            std::cout << "acceptor source stopped with ec: " << error.message() << " (" << error.category().name() << ": " << error.value() << ")" << std::endl;
            static_cast<std::promise<void>*>(ctx)->set_value();
        },
        &close_promise);

    ASSERT_NE(accept_source, nullptr);

    boost::asio::io_context context;
    std::vector<std::jthread> threads;
    threads.reserve(g_connect_thread_count);
    for (std::size_t i = 0; i < g_connect_thread_count; ++i) {
        threads.emplace_back([&] {
            for (std::size_t j = 0; j < g_connect_action_count; ++j) {
                boost::asio::ip::tcp::socket sock(context);
                sock.connect(boost::asio::ip::tcp::endpoint(
                    boost::asio::ip::make_address("127.0.0.1"), 5656));
                // std::this_thread::sleep_for(std::chrono::milliseconds { 100 });
            }
        });
    }

    future.wait();

    ntt_accept_source_stop(accept_source);

    close_future.wait();

    ntt_accept_source_delete(accept_source);

    ntt_pool_release(pool);

    ntt_sockaddr_release(sockaddr);

    f.wait();

    auto after = ntt_accept_source_get_wakeups_count();
    std::cout << after - before << std::endl;
}

TEST_F(AcceptSourceTest, Connect)
{
    ntt::context context { 8 };

    auto addr
        = ntt_sockaddr_make_from_view(
            ntt_view_from_literal("127.0.0.1:5656"));

    auto* accept_source
        = ntt_accept_source_create(
            context.as_pool(),
            addr);

    std::promise<void> accepted_promise;
    auto accepted_future = accepted_promise.get_future();

    std::promise<void> stopped_promise;
    auto stopped_future = stopped_promise.get_future();

    ntt_accept_source_start(
        accept_source,
        +[](void* ctx, int fd) {
            close(fd);
            static_cast<std::promise<void>*>(ctx)->set_value();
        },
        &accepted_promise,
        +[](int ec, void* ctx) {
            auto error = std::make_error_code(static_cast<std::errc>(ec));
            std::cout << "acceptor source stopped with ec: " << error.message() << " (" << error.category().name() << ": " << error.value() << ")" << std::endl;
            static_cast<std::promise<void>*>(ctx)->set_value();
        },
        &stopped_promise);

    std::promise<void> connected_promise;
    auto connected_future = connected_promise.get_future();
    auto* connect_request = ntt_connect_request_create();
    ntt_connect_request_do(
        connect_request, context.as_pool(),
        addr,
        +[](void* ctx, int ec, int sock, ntt_sockaddr_t* local_addr, ntt_sockaddr_t* remote_addr) {
            if (ec != 0) {
                auto error = std::make_error_code(static_cast<std::errc>(ec));
                std::cout << "connect request failed with ec: " << error.message() << " (" << error.category().name() << ": " << error.value() << ")" << std::endl;
            } else {
                std::cout << "connected" << std::endl;
                close(sock);
            }
            static_cast<std::promise<void>*>(ctx)->set_value();
        },
        &connected_promise);

    accepted_future.wait();
    connected_future.wait();

    ntt_connect_request_delete(connect_request);

    ntt_accept_source_stop(accept_source);
    stopped_future.wait();
    ntt_accept_source_delete(accept_source);
}

TEST_F(AcceptSourceTest, EpollHupWakeupCheck)
{
    auto sockaddr = ntt_sockaddr_make_from_view(ntt_view_from_literal("0.0.0.0:5656"));
    ASSERT_NE(sockaddr, nullptr);

    int type = SOCK_CLOEXEC | SOCK_STREAM | SOCK_NONBLOCK;
    auto sock = socket(sockaddr->storage.ss_family, type, 0);
    ASSERT_NE(sock, -1);

    int v = 1;
    int rc = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(int));
    ASSERT_NE(rc, -1);

    rc = bind(sock, (struct sockaddr*)&sockaddr->storage,
        sizeof(struct sockaddr_in));
    ASSERT_NE(rc, -1);

    rc = listen(sock, SOMAXCONN);
    ASSERT_NE(rc, -1);

    int epollfd = epoll_create1(EPOLL_CLOEXEC);
    ASSERT_NE(rc, -1);

    struct epoll_event event;
    event.data.ptr = NULL;
    event.events = EPOLLIN | EPOLLET;

    rc = epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &event);
    ASSERT_NE(rc, -1);

    shutdown(sock, SHUT_RD);

    struct epoll_event ev;
    rc = epoll_wait(epollfd, &ev, 1, -1);

    ASSERT_TRUE((ev.events & EPOLLHUP) != 0);

    //   rc = epoll_wait(epollfd, &ev, 1, -1);
}

TEST_F(AcceptSourceTest, EpollHupReadErrorCheck)
{
    auto sockaddr = ntt_sockaddr_make_from_view(ntt_view_from_literal("0.0.0.0:5656"));
    ASSERT_NE(sockaddr, nullptr);

    int type = SOCK_CLOEXEC | SOCK_STREAM | SOCK_NONBLOCK;
    auto sock = socket(sockaddr->storage.ss_family, type, 0);
    ASSERT_NE(sock, -1);

    int v = 1;
    int rc = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(int));
    ASSERT_NE(rc, -1);

    rc = bind(sock, (struct sockaddr*)&sockaddr->storage,
        sizeof(struct sockaddr_in));
    ASSERT_NE(rc, -1);

    rc = listen(sock, SOMAXCONN);
    ASSERT_NE(rc, -1);

    int epollfd = epoll_create1(EPOLL_CLOEXEC);
    ASSERT_NE(rc, -1);

    struct epoll_event event;
    event.data.ptr = NULL;
    event.events = EPOLLIN | EPOLLET;

    rc = epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &event);
    ASSERT_NE(rc, -1);

    shutdown(sock, SHUT_RD);

    int fd = accept(sock, NULL, 0);
    ASSERT_EQ(fd, -1);

    struct epoll_event ev;
    rc = epoll_wait(epollfd, &ev, 1, -1);

    ASSERT_TRUE((ev.events & EPOLLHUP) != 0);

    //   rc = epoll_wait(epollfd, &ev, 1, -1);
}

TEST_F(AcceptSourceTest, EpollHupErrorAfterConnect)
{
    auto sockaddr = ntt_sockaddr_make_from_view(ntt_view_from_literal("0.0.0.0:5656"));
    ASSERT_NE(sockaddr, nullptr);

    int type = SOCK_CLOEXEC | SOCK_STREAM | SOCK_NONBLOCK;
    auto sock = socket(sockaddr->storage.ss_family, type, 0);
    ASSERT_NE(sock, -1);

    int v = 1;
    int rc = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(int));
    ASSERT_NE(rc, -1);

    rc = bind(sock, (struct sockaddr*)&sockaddr->storage,
        sizeof(struct sockaddr_in));
    ASSERT_NE(rc, -1);

    rc = listen(sock, SOMAXCONN);
    ASSERT_NE(rc, -1);

    int epollfd = epoll_create1(EPOLL_CLOEXEC);
    ASSERT_NE(rc, -1);

    struct epoll_event event;
    event.data.ptr = NULL;
    event.events = EPOLLIN | EPOLLET;

    rc = epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &event);
    ASSERT_NE(rc, -1);

    boost::asio::io_context context;
    boost::asio::ip::tcp::socket csock(context);
    csock.connect(boost::asio::ip::tcp::endpoint(
        boost::asio::ip::make_address("127.0.0.1"), 5656));

    shutdown(sock, SHUT_RD);

    int fd = accept(sock, NULL, 0);
    ASSERT_EQ(fd, -1);

    struct epoll_event ev;
    rc = epoll_wait(epollfd, &ev, 1, -1);

    ASSERT_TRUE((ev.events & EPOLLHUP) != 0);

    // rc = epoll_wait(epollfd, &ev, 1, -1);
}

class Acceptor {
public:
    Acceptor(ntt_pool_t* pool, const char* cstr)
        : m_pool { pool }
        , m_accept_source {
            ntt_accept_source_create(
                pool,
                ntt_sockaddr_make_from_view(ntt_view_from_cstr(cstr)))
        }
    {
    }

    void start()
    {
        ntt_accept_source_start(
            m_accept_source,
            +[](void* ctx, int sock) {
                static_cast<Acceptor*>(ctx)->accepted(sock);
            },
            this,
            +[](int ec, void* ctx) {
                static_cast<Acceptor*>(ctx)->stopped(std::make_error_code(std::errc { ec }));
            },
            this);
    }

    void stop()
    {
        ntt_accept_source_stop(m_accept_source);
    }

    void accepted(int sock)
    {
        auto* session = ntt_tcp_source_create(m_pool);
        m_sessions.emplace(session);
        ntt_tcp_source_start(session, sock);
    }

    void stopped(std::error_code ec)
    {
        std::cout << "acceptor source stopped with ec: " << ec.message() << " (" << ec.category().name() << ": " << ec.value() << ")" << std::endl;
        p.set_value();
    }

    ~Acceptor()
    {
        f.wait();
    }

private:
    std::set<ntt_tcp_source_t*> m_sessions;
    std::promise<void> p;
    std::future<void> f = p.get_future();
    ntt_pool_t* m_pool;
    ntt_accept_source_t* m_accept_source;
};

