#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <chrono>
#include <ntt/cqueue.h>
#include <ntt/signal.h>

#include <boost/asio.hpp>

#include <gtest/gtest.h>
#include <random>
#include <thread>

struct Node {
  Node() { ntt_node_init(&node); }

  ~Node() {}

  static Node *from(struct ntt_node *ntt_node) {
    return reinterpret_cast<Node *>(reinterpret_cast<uint8_t *>(ntt_node) -
                                    offsetof(Node, node));
  }

  struct ntt_node node;
};

TEST(Queue, Example) {
  static constexpr std::size_t g_consumer_cnt = 4;
  static constexpr std::size_t g_producer_cnt = 4;
  static constexpr std::size_t g_iteration_cnt = 100'000;

  int rc;

  struct ntt_cqueue queue;
  ntt_cqueue_init(&queue);

  struct ntt_signal signal;
  ntt_signal_init(&signal, NTT_SIGNAL_CV);

  std::vector<std::thread> producers;
  for (std::size_t i = 0; i < g_producer_cnt; ++i) {
    producers.emplace_back([signal = &signal, queue = &queue] {
      std::random_device rd;
      std::mt19937 engine(rd());
      std::uniform_int_distribution<int> dist(5, 20);
      for (std::size_t i = 0; i < g_iteration_cnt; ++i) {
        std::this_thread::sleep_for(std::chrono::microseconds(dist(engine)));
        ntt_cqueue_push(queue, &(new Node)->node);
        ntt_signal_notify(signal);
      }
    });
  }

  std::vector<std::thread> consumers;
  for (std::size_t i = 0; i < g_consumer_cnt; ++i) {
    consumers.emplace_back([queue = &queue, signal = &signal]() {
      while (ntt_signal_wait(signal) == 0) {
        delete Node::from(ntt_cqueue_pop(queue));
      }
    });
  }

  for (auto &thread : producers) {
    thread.join();
  }

  ntt_signal_cancel(
      &signal, nullptr, +[](void *) { std::cout << "stopped" << std::endl; });

  for (auto &thread : consumers) {
    thread.join();
  }
}
/*
void foo(struct ntt_root *root) {
  static constexpr std::size_t g_iteration_cnt = 1'000'000;

  struct ntt_queue queue;
  ntt_queue_init(&queue, root); // root queue reference += 1

  for (std::size_t i = 0; i < g_iteration_cnt; ++i) {
  }
}

TEST(Ntt, Example) {
  struct ntt_root root;
  ntt_root_init(&root); // root queue reference = 1
  foo(&root);
  ntt_root_destroy(&root); // root queue reference -= 1
}
*/

boost::asio::ip::tcp::endpoint
open_and_bind_acceptor(boost::asio::ip::tcp::acceptor &acceptor,
                       const boost::asio::ip::tcp::endpoint &bind_address,
                       boost::system::error_code &ec) {
  boost::asio::ip::tcp::endpoint endpoint;

  if (ec = acceptor.open(bind_address.protocol(), ec); !ec) {
    if (ec = acceptor.set_option(
            boost::asio::ip::tcp::socket::reuse_address(true), ec);
        !ec) {
      if (ec = acceptor.bind(bind_address, ec); !ec) {
        if (ec = acceptor.listen(
                boost::asio::ip::tcp::acceptor::max_listen_connections, ec);
            !ec) {
          return acceptor.local_endpoint(ec);
        }
      }
    }
  }

  return endpoint;
}
std::pair<boost::asio::ip::tcp::acceptor, boost::asio::ip::tcp::endpoint>
make_listen_acceptor(boost::asio::io_context &io_context,
                     const boost::asio::ip::tcp::endpoint &bind_address,
                     boost::system::error_code &ec) {
  boost::system::error_code _ec;
  boost::asio::ip::tcp::acceptor acceptor(io_context);
  boost::asio::ip::tcp::endpoint endpoint =
      open_and_bind_acceptor(acceptor, bind_address, _ec);
  ec = _ec;
  return {std::move(acceptor), std::move(endpoint)};
}

std::pair<boost::asio::ip::tcp::acceptor, boost::asio::ip::tcp::endpoint>
make_listen_acceptor(boost::asio::io_context &io_context, std::uint16_t port,
                     boost::system::error_code &ec) {
  return make_listen_acceptor(
      io_context,
      boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(),
                                     port),
      ec);
}

std::pair<boost::asio::ip::tcp::acceptor, boost::asio::ip::tcp::endpoint>
make_listen_acceptor(boost::asio::io_context &io_context, std::uint16_t port) {
  boost::system::error_code ec;
  auto result = make_listen_acceptor(io_context, port, ec);
  if (ec) {
    throw std::system_error(ec);
  }
  return result;
}

TEST(BeastTcpStream, ReadSyncWriteAsync) {
  boost::asio::io_context io_context;
  boost::system::error_code ec;
  auto [acceptor, server_endpoint] = make_listen_acceptor(io_context, 0);
  boost::asio::ip::tcp::socket client_socket{io_context};
  auto connect_future =
      client_socket.async_connect(server_endpoint, boost::asio::use_future);
  auto server_socket_future = acceptor.async_accept(boost::asio::use_future);

  io_context.run_one();
  auto server_socket = server_socket_future.get();

  io_context.run_one();
  connect_future.get();

  {
    std::array<std::uint8_t, 256> tx;
    auto transfer_bytes = server_socket.write_some(
        boost::asio::const_buffer{tx.data(), tx.size()});
    ASSERT_EQ(transfer_bytes, tx.size());
  }

  std::array<std::uint8_t, 16> rx{};
  io_context.post([&] {
    client_socket.async_receive(
        boost::asio::mutable_buffer{rx.data(), rx.size()},
        [](auto ec, auto bytes_transferred) {
          std::abort();
          std::cout << "done" << std::endl;
        });
  });
  io_context.run();
  std::cout << "WTF" << std::endl;
  // ASSERT_EQ(transfer_bytes_future.get(), rx.size());
}