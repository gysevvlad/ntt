#include "ntt/loop.h"
#include "ntt/squeue.h"
#include "ntt/task_queue.h"
#include "ntt/thread_pool.h"

#include <algorithm>
#include <atomic>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <gtest/gtest.h>
#include <random>
#include <thread>
#include <type_traits>
#include <utility>

template <class Functor> struct Task {
  static void on_action(struct ntt_epoll_fd *epoll_fd,
                        struct ntt_event_loop *) {
    Task *task = ntt_container_of(epoll_fd, Task, fd);
    task->m_func();
    delete task;
  }

  static_assert(std::is_same_v<std::decay_t<Functor>, Functor>);

  template <class In> Task(In &&func) : m_func(std::forward<In>(func)) {}

  Functor m_func;

  ntt_epoll_fd fd = ntt_epoll_fd{
      .on_events = nullptr,
      .on_action = &on_action,
      .actions_node = {},
  };
};

template <class Functor> ntt_epoll_fd *make_action(Functor &&functor) {
  auto *task = new Task<std::decay_t<Functor>>(std::forward<Functor>(functor));
  return &task->fd;
}

TEST(Ntt, DISABLED_loop) {
  ntt_event_loop loop;
  ntt_event_loop_init(&loop);

  std::thread thread([&loop] { ntt_event_loop_run(&loop); });

  std::size_t cnt = 0;
  std::vector<ntt_epoll_fd *> data;
  data.reserve(1'000'000);
  for (std::size_t i = 0; i < 1'000'000; ++i) {
    data.emplace_back(make_action([&cnt] { cnt += 1; }));
  }
  for (auto d : data) {
    ntt_event_loop_action(&loop, d);
  }
  ntt_event_loop_action(&loop,
                        make_action([&loop] { loop.active_events -= 1; }));
  thread.join();
  std::cout << cnt << std::endl;
}

TEST(ntt, thread_pool) {
  static constexpr std::size_t g_expected_width = 8;
  std::promise<void> p;
  auto f = p.get_future();
  std::atomic<std::size_t> w = 0;
  ntt_thread_pool thread_pool;
  ntt_thread_pool_init(
      &thread_pool, g_expected_width,
      [](void *arg) {
        static_cast<std::atomic<std::size_t> *>(arg)->fetch_add(1);
        return 0;
      },
      &w,
      [](void *arg) { static_cast<std::promise<void> *>(arg)->set_value(); },
      &p);
  ntt_thread_pool_destroy(&thread_pool);
  f.wait();
  ASSERT_EQ(g_expected_width, w.load());
}

void run(std::shared_ptr<ntt_thread_pool> thread_pool) {
  struct Server {
    Server(std::shared_ptr<ntt_thread_pool> thread_pool)
        : m_thread_pool(std::move(thread_pool)) {}

    void start() {}

    std::shared_ptr<ntt_thread_pool> m_thread_pool;
  };

  auto server = std::make_shared<Server>(std::move(thread_pool));
  server->start();
}

TEST(ntt, thread_pool_self_destroy) {
  static constexpr std::size_t g_expected_width = 8;

  // prepare runtime
  std::promise<void> p;
  auto f = p.get_future();
  std::shared_ptr<ntt_thread_pool> thread_pool(new ntt_thread_pool,
                                               [](ntt_thread_pool *v) {
                                                 ntt_thread_pool_destroy(v);
                                                 delete v;
                                               });
  ntt_thread_pool_init(
      thread_pool.get(), g_expected_width, [](void *arg) { return 0; }, nullptr,
      [](void *arg) { static_cast<std::promise<void> *>(arg)->set_value(); },
      &p);

  run(std::move(thread_pool));

  f.wait();
}

template <class Functor> struct TaskNode : Functor {
  static void svc(void *arg) {
    auto task_node = static_cast<TaskNode *>(arg);
    (*task_node)();
    delete task_node;
  }
  static_assert(std::is_same_v<std::decay_t<Functor>, Functor>);
  template <class In> TaskNode(In &&func) : Functor(std::forward<In>(func)) {
    node.svc = svc;
    node.arg = this;
  }
  ntt_task_node node;
};

template <class Functor> ntt_task_node *make_task(Functor &&functor) {
  auto node =
      new TaskNode<std::decay_t<Functor>>(std::forward<Functor>(functor));
  return &node->node;
}

template <class Functor> struct SerialQueueTaskNode : Functor {
  static void svc(void *arg) {
    auto task_node = static_cast<SerialQueueTaskNode *>(arg);
    (*task_node)();
    delete task_node;
  }
  static_assert(std::is_same_v<std::decay_t<Functor>, Functor>);
  template <class In>
  SerialQueueTaskNode(In &&func) : Functor(std::forward<In>(func)) {
    serial_queue_task.svc = svc;
    serial_queue_task.arg = this;
  }
  ntt_sq_task serial_queue_task;
};

template <class Functor> ntt_sq_task *make_sq_task(Functor &&functor) {
  auto node = new SerialQueueTaskNode<std::decay_t<Functor>>(
      std::forward<Functor>(functor));
  return &node->serial_queue_task;
}

TEST(ntt, task_queue) {
  static constexpr std::size_t g_width = 8;
  std::promise<void> p;
  auto f = p.get_future();

  ntt_task_queue task_queue;
  ntt_task_queue_init(&task_queue);

  ntt_thread_pool thread_pool;
  ntt_thread_pool_init(
      &thread_pool, g_width,
      [](void *arg) {
        auto *task_queue = static_cast<ntt_task_queue *>(arg);
        struct ntt_task_node *node = NULL;
        while ((node = ntt_task_queue_pop(task_queue))) {
          node->svc(node->arg);
        }
        return 0;
      },
      &task_queue,
      [](void *arg) { static_cast<std::promise<void> *>(arg)->set_value(); },
      &p);

  static constexpr std::size_t g_expected_cnt = 1'000'000;
  std::atomic<std::size_t> cnt = 0;
  std::promise<void> done;
  auto future = done.get_future();
  ntt_task_queue_push(&task_queue, make_task([&] {
    for (std::size_t i = 0; i < g_expected_cnt; ++i) {
      ntt_task_queue_push(&task_queue, make_task([&] {
        for (std::size_t i = 0; i < g_expected_cnt; ++i) {
          __asm__("nop");
        }
        if (cnt.fetch_add(1) == g_expected_cnt - 1) {
          done.set_value();
        }
      }));
    }
  }));

  future.wait();
  ntt_task_queue_stop(&task_queue);
  ntt_thread_pool_destroy(&thread_pool);
  f.wait();
  ntt_task_queue_destroy(&task_queue);
}

TEST(ntt, sq) {
  static constexpr std::size_t g_width = 8;
  std::promise<void> p;
  auto f = p.get_future();

  ntt_task_queue task_queue;
  ntt_task_queue_init(&task_queue);

  ntt_squeue sq1;
  ntt_squeue_init(&sq1, &task_queue);

  ntt_squeue sq2;
  ntt_squeue_init(&sq2, &task_queue);

  ntt_thread_pool thread_pool;
  ntt_thread_pool_init(
      &thread_pool, g_width,
      [](void *arg) {
        auto *task_queue = static_cast<ntt_task_queue *>(arg);
        struct ntt_task_node *node = NULL;
        while ((node = ntt_task_queue_pop(task_queue))) {
          node->svc(node->arg);
        }
        return 0;
      },
      &task_queue,
      [](void *arg) { static_cast<std::promise<void> *>(arg)->set_value(); },
      &p);

  static constexpr std::size_t g_expected_cnt = 100'000'000;
  std::atomic<std::size_t> cnt = 0;
  std::promise<void> done;
  auto future = done.get_future();
  ntt_squeue_dispatch(&sq1, make_sq_task([&] {
    for (std::size_t i = 0; i < g_expected_cnt / 2; ++i) {
      ntt_squeue_dispatch(&sq1, make_sq_task([&] {
        if (cnt.fetch_add(1) == g_expected_cnt - 1) {
          std::cout << "done" << std::endl;
          done.set_value();
        }
      }));
    }
  }));
  ntt_squeue_dispatch(&sq2, make_sq_task([&] {
    for (std::size_t i = 0; i < g_expected_cnt / 2; ++i) {
      ntt_squeue_dispatch(&sq2, make_sq_task([&] {
        if (cnt.fetch_add(1) == g_expected_cnt - 1) {
          std::cout << "done" << std::endl;
          done.set_value();
        }
      }));
    }
  }));

  future.wait();
  ntt_task_queue_stop(&task_queue);
  ntt_thread_pool_destroy(&thread_pool);
  f.wait();
  ntt_task_queue_destroy(&task_queue);
}

TEST(ntt, stand) {
  static constexpr std::size_t g_width = 8;
  std::promise<void> p;
  auto f = p.get_future();

  /// cnt = x
  /// main -> recv queue (n) -> proc queue (m) -> send queue (k)
  /// 1. f(x, n) - fn[x]
  /// 2. f(x, m) - fm[x]
  /// 3. f(x, k) - fk[x]
  /// 4. fnw[x]
  /// 4. fnw[x]
  /// 4. fnw[x]

  static constexpr std::size_t g_cnt = 1'000'000;
  static constexpr std::size_t n = 20;
  static constexpr std::size_t m = 1000;
  static constexpr std::size_t k = 4;
  static constexpr std::chrono::microseconds g_wakeup{10};
  static constexpr std::chrono::microseconds g_wakeup_noise{2};
  static constexpr std::chrono::microseconds g_recv_work{5};
  static constexpr std::chrono::microseconds g_recv_noise{2};
  static constexpr std::chrono::microseconds g_proc_work{10};
  static constexpr std::chrono::microseconds g_proc_noise{2};

  struct ctx {
    std::vector<std::size_t> fn = std::vector<std::size_t>(g_cnt);
    std::vector<std::size_t> fm = std::vector<std::size_t>(g_cnt);
    std::vector<std::size_t> fk = std::vector<std::size_t>(g_cnt);

    std::vector<std::chrono::microseconds> fnw{g_cnt};
    std::vector<std::chrono::microseconds> fmw{g_cnt};
    std::vector<std::chrono::microseconds> fkw{g_cnt};

    std::vector<ntt_squeue> recv{n};
    std::vector<ntt_squeue> proc{m};
    std::vector<ntt_squeue> send{k};

    std::vector<std::chrono::high_resolution_clock::time_point> in{g_cnt};
    std::vector<std::chrono::high_resolution_clock::time_point> out{g_cnt};

    std::promise<void> done;
    std::future<void> f = done.get_future();
    std::atomic<std::size_t> done_cnt = 0;
  } ctx;

  std::mt19937 gen({1234});
  std::uniform_int_distribution<std::size_t> fnd(0, n - 1);
  for (std::size_t i = 0; i < g_cnt; ++i) {
    ctx.fn[i] = fnd(gen);
  }
  std::uniform_int_distribution<std::size_t> fmd(0, m - 1);
  for (std::size_t i = 0; i < g_cnt; ++i) {
    ctx.fm[i] = fmd(gen);
  }
  std::uniform_int_distribution<std::size_t> fkd(0, k - 1);
  for (std::size_t i = 0; i < g_cnt; ++i) {
    ctx.fk[i] = fkd(gen);
  }
  std::uniform_int_distribution<std::size_t> fnwd(
      (g_wakeup - g_wakeup_noise).count(), (g_wakeup + g_wakeup_noise).count());
  for (std::size_t i = 0; i < g_cnt; ++i) {
    ctx.fnw[i] = std::chrono::microseconds{fnwd(gen)};
  }
  std::uniform_int_distribution<std::size_t> fmwd(
      (g_recv_work - g_recv_noise).count(),
      (g_recv_work + g_recv_noise).count());
  for (std::size_t i = 0; i < g_cnt; ++i) {
    ctx.fmw[i] = std::chrono::microseconds{fmwd(gen)};
  }
  std::uniform_int_distribution<std::size_t> fkwd(
      (g_proc_work - g_proc_noise).count(),
      (g_proc_work + g_proc_noise).count());
  for (std::size_t i = 0; i < g_cnt; ++i) {
    ctx.fkw[i] = std::chrono::microseconds{fkwd(gen)};
  }

  ntt_task_queue task_queue;
  ntt_task_queue_init(&task_queue);

  for (auto &group : {&ctx.recv, &ctx.proc, &ctx.send}) {
    for (auto &queue : *group) {
      ntt_squeue_init(&queue, &task_queue);
    }
  }

  ntt_thread_pool thread_pool;
  ntt_thread_pool_init(
      &thread_pool, g_width,
      [](void *arg) {
        auto *task_queue = static_cast<ntt_task_queue *>(arg);
        struct ntt_task_node *node = NULL;
        while ((node = ntt_task_queue_pop(task_queue))) {
          node->svc(node->arg);
        }
        return 0;
      },
      &task_queue,
      [](void *arg) { static_cast<std::promise<void> *>(arg)->set_value(); },
      &p);

  for (std::size_t i = 0; i < g_cnt; ++i) {
    int x = i;
    std::this_thread::sleep_for(ctx.fnw[x]);
    ctx.in[x] = std::chrono::high_resolution_clock::now();
    ntt_squeue_dispatch(&ctx.recv[ctx.fn[x]], make_sq_task([ctx = &ctx, x = i] {
      std::this_thread::sleep_for(ctx->fmw[x]);
      // we are in recv queue
      ntt_squeue_dispatch(&ctx->proc[ctx->fm[x]], make_sq_task([ctx, x = x] {
        std::this_thread::sleep_for(ctx->fkw[x]);
        // we are in proc queue
        ntt_squeue_dispatch(&ctx->send[ctx->fk[x]], make_sq_task([ctx, x = x] {
          // we are in send queue
          ctx->out[x] = std::chrono::high_resolution_clock::now();
          if (ctx->done_cnt.fetch_add(1) == g_cnt - 1) {
            ctx->done.set_value();
          }
        }));
      }));
    }));
  }

  ctx.f.wait();
  ntt_task_queue_stop(&task_queue);
  ntt_thread_pool_destroy(&thread_pool);
  f.wait();
  ntt_task_queue_destroy(&task_queue);

  std::vector<std::chrono::high_resolution_clock::duration> res(g_cnt);
  for (std::size_t i = 0; i < g_cnt; ++i) {
    res[i] = ctx.out[i] - ctx.in[i];
  }
  std::sort(res.begin(), res.end());
  std::cout << "50%% " << res[static_cast<int>(res.size() * 0.5)].count() / 1000
            << std::endl;
  std::cout << "80%% " << res[static_cast<int>(res.size() * 0.8)].count() / 1000
            << std::endl;
  std::cout << "95%% "
            << res[static_cast<int>(res.size() * 0.95)].count() / 1000
            << std::endl;
}

TEST(boost, stand) {
  static constexpr std::size_t g_width = 16;
  std::promise<void> p;
  auto f = p.get_future();

  /// cnt = x
  /// main -> recv queue (n) -> proc queue (m) -> send queue (k)
  /// 1. f(x, n) - fn[x]
  /// 2. f(x, m) - fm[x]
  /// 3. f(x, k) - fk[x]
  /// 4. fnw[x]
  /// 4. fnw[x]
  /// 4. fnw[x]

  // static constexpr std::size_t g_cnt = 1'000'000;
  static constexpr std::size_t g_cnt = 1'000;
  static constexpr std::size_t n = 20;
  static constexpr std::size_t m = 1000;
  static constexpr std::size_t k = 4;
  static constexpr std::chrono::microseconds g_wakeup{100};
  static constexpr std::chrono::microseconds g_wakeup_noise{2};
  static constexpr std::chrono::microseconds g_recv_work{5};
  static constexpr std::chrono::microseconds g_recv_noise{2};
  static constexpr std::chrono::microseconds g_proc_work{10};
  static constexpr std::chrono::microseconds g_proc_noise{2};

  boost::asio::io_context context;

  struct ctx {
    std::vector<std::size_t> fn = std::vector<std::size_t>(g_cnt);
    std::vector<std::size_t> fm = std::vector<std::size_t>(g_cnt);
    std::vector<std::size_t> fk = std::vector<std::size_t>(g_cnt);

    std::vector<std::chrono::microseconds> fnw{g_cnt};
    std::vector<std::chrono::microseconds> fmw{g_cnt};
    std::vector<std::chrono::microseconds> fkw{g_cnt};

    using queue_t = boost::asio::strand<boost::asio::io_context::executor_type>;
    std::deque<queue_t> recv;
    std::deque<queue_t> proc;
    std::deque<queue_t> send;

    std::vector<std::chrono::high_resolution_clock::time_point> in{g_cnt};
    std::vector<std::chrono::high_resolution_clock::time_point> out{g_cnt};

    std::promise<void> done;
    std::future<void> f = done.get_future();
    std::atomic<std::size_t> done_cnt = 0;
  } ctx;

  std::mt19937 gen({1234});
  std::uniform_int_distribution<std::size_t> fnd(0, n - 1);
  for (std::size_t i = 0; i < g_cnt; ++i) {
    ctx.fn[i] = fnd(gen);
  }
  std::uniform_int_distribution<std::size_t> fmd(0, m - 1);
  for (std::size_t i = 0; i < g_cnt; ++i) {
    ctx.fm[i] = fmd(gen);
  }
  std::uniform_int_distribution<std::size_t> fkd(0, k - 1);
  for (std::size_t i = 0; i < g_cnt; ++i) {
    ctx.fk[i] = fkd(gen);
  }
  std::uniform_int_distribution<std::size_t> fnwd(
      (g_wakeup - g_wakeup_noise).count(), (g_wakeup + g_wakeup_noise).count());
  for (std::size_t i = 0; i < g_cnt; ++i) {
    ctx.fnw[i] = std::chrono::microseconds{fnwd(gen)};
  }
  std::uniform_int_distribution<std::size_t> fmwd(
      (g_recv_work - g_recv_noise).count(),
      (g_recv_work + g_recv_noise).count());
  for (std::size_t i = 0; i < g_cnt; ++i) {
    ctx.fmw[i] = std::chrono::microseconds{fmwd(gen)};
  }
  std::uniform_int_distribution<std::size_t> fkwd(
      (g_proc_work - g_proc_noise).count(),
      (g_proc_work + g_proc_noise).count());
  for (std::size_t i = 0; i < g_cnt; ++i) {
    ctx.fkw[i] = std::chrono::microseconds{fkwd(gen)};
  }

  for (std::size_t i = 0; i < g_cnt; ++i) {
    ctx.recv.emplace_back(context.get_executor());
    ctx.proc.emplace_back(context.get_executor());
    ctx.send.emplace_back(context.get_executor());
  }

  auto work_guard = boost::asio::make_work_guard(context);
  std::vector<std::thread> thread_pool;
  for (std::size_t i = 0; i < g_width; ++i) {
    thread_pool.emplace_back([&context] { context.run(); });
  }

  for (std::size_t i = 0; i < g_cnt; ++i) {
    int x = i;
    std::this_thread::sleep_for(ctx.fnw[x]);
    ctx.in[x] = std::chrono::high_resolution_clock::now();
    boost::asio::dispatch(ctx.recv[ctx.fn[x]], [ctx = &ctx, x] {
      std::this_thread::sleep_for(ctx->fmw[x]);
      // we are in recv queue
      boost::asio::dispatch(ctx->proc[ctx->fm[x]], [ctx, x = x] {
        std::this_thread::sleep_for(ctx->fkw[x]);
        // we are in proc queue
        boost::asio::dispatch(ctx->send[ctx->fk[x]], [ctx, x = x] {
          // we are in send queue
          ctx->out[x] = std::chrono::high_resolution_clock::now();
          if (ctx->done_cnt.fetch_add(1) == g_cnt - 1) {
            ctx->done.set_value();
          }
        });
      });
    });
  }

  ctx.f.wait();
  work_guard.reset();
  for (auto &thread : thread_pool) {
    thread.join();
  }

  std::vector<std::chrono::high_resolution_clock::duration> res(g_cnt);
  for (std::size_t i = 0; i < g_cnt; ++i) {
    res[i] = ctx.out[i] - ctx.in[i];
  }
  std::sort(res.begin(), res.end());
  std::cout << "50%% " << res[static_cast<int>(res.size() * 0.5)].count() / 1000
            << std::endl;
  std::cout << "80%% " << res[static_cast<int>(res.size() * 0.8)].count() / 1000
            << std::endl;
  std::cout << "95%% "
            << res[static_cast<int>(res.size() * 0.95)].count() / 1000
            << std::endl;
}
