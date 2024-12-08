#include "ntt/accept_source.h"
#include "ntt/ntt.hpp"

#include "ntt/impl/accept_source.h"
#include "ntt/impl/sockaddr.h"

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
#include <thread>

class AcceptSourceTest : public ::testing::Test {};

TEST_F(AcceptSourceTest, Common) {
  auto pool = ntt_pool_create(2);
  ASSERT_NE(pool, nullptr);

  auto sockaddr =
      ntt_sockaddr_make_from_view(ntt_view_from_literal("0.0.0.0:5656"));
  ASSERT_NE(sockaddr, nullptr);

  std::promise<void> promise;
  std::future<void> future = promise.get_future();

  auto accept_source = ntt_accept_source_create(
      pool,
      +[](void *ctx, int fd) {
        static_cast<std::promise<void> *>(ctx)->set_value();
        close(fd);
      },
      &promise, +[](void *) {}, nullptr, sockaddr);
  ASSERT_NE(accept_source, nullptr);

  boost::asio::io_context context;
  boost::asio::ip::tcp::socket sock(context);
  sock.connect(boost::asio::ip::tcp::endpoint(
      boost::asio::ip::make_address("127.0.0.1"), 5656));

  future.wait();

  ntt_accept_source_cancel(accept_source);
}

TEST_F(AcceptSourceTest, EpollHupWakeupCheck) {
  auto sockaddr =
      ntt_sockaddr_make_from_view(ntt_view_from_literal("0.0.0.0:5656"));
  ASSERT_NE(sockaddr, nullptr);

  int type = SOCK_CLOEXEC | SOCK_STREAM | SOCK_NONBLOCK;
  auto sock = socket(sockaddr->storage.ss_family, type, 0);
  ASSERT_NE(sock, -1);

  int v = 1;
  int rc = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(int));
  ASSERT_NE(rc, -1);

  rc = bind(sock, (struct sockaddr *)&sockaddr->storage,
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

TEST_F(AcceptSourceTest, EpollHupReadErrorCheck) {
  auto sockaddr =
      ntt_sockaddr_make_from_view(ntt_view_from_literal("0.0.0.0:5656"));
  ASSERT_NE(sockaddr, nullptr);

  int type = SOCK_CLOEXEC | SOCK_STREAM | SOCK_NONBLOCK;
  auto sock = socket(sockaddr->storage.ss_family, type, 0);
  ASSERT_NE(sock, -1);

  int v = 1;
  int rc = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(int));
  ASSERT_NE(rc, -1);

  rc = bind(sock, (struct sockaddr *)&sockaddr->storage,
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

TEST_F(AcceptSourceTest, EpollHupErrorAfterConnect) {
  auto sockaddr =
      ntt_sockaddr_make_from_view(ntt_view_from_literal("0.0.0.0:5656"));
  ASSERT_NE(sockaddr, nullptr);

  int type = SOCK_CLOEXEC | SOCK_STREAM | SOCK_NONBLOCK;
  auto sock = socket(sockaddr->storage.ss_family, type, 0);
  ASSERT_NE(sock, -1);

  int v = 1;
  int rc = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(int));
  ASSERT_NE(rc, -1);

  rc = bind(sock, (struct sockaddr *)&sockaddr->storage,
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

  //   rc = epoll_wait(epollfd, &ev, 1, -1);
}
