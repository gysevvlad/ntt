#include "ntt/loop.h"
#include "ntt/defs.h"
#include "ntt/event_queue.h"
#include "ntt/mpsc_queue.h"
#include "ntt/node.h"
#include "ntt/task_queue.h"
#include "ntt/thread_pool.h"
#include "ntt/work_loop.h"

#include <algorithm>
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
#include <urcu/wfcqueue.h>
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

template <class Functor> struct Event : Functor {
  static void svc(void *arg) {
    auto event = static_cast<Event *>(arg);
    (*event)();
    delete event;
  }
  static_assert(std::is_same_v<std::decay_t<Functor>, Functor>);
  template <class In> Event(In &&func) : Functor(std::forward<In>(func)) {
    event.svc_cb = svc;
    event.svc_arg = this;
  }
  ntt_event event;
};

template <class Functor> ntt_event *make_event(Functor &&functor) {
  auto event = new Event<std::decay_t<Functor>>(std::forward<Functor>(functor));
  return &event->event;
}

template <class Functor> struct SerialQueueTaskNode {
  static void svc(void *arg) {
    auto task_node = static_cast<SerialQueueTaskNode *>(arg);
    task_node->m_func();
    task_node->m_func.~Functor();
  }
  static void del(void *arg);
  static_assert(std::is_same_v<std::decay_t<Functor>, Functor>);
  template <class In>
  SerialQueueTaskNode(In &&func) : m_func(std::forward<In>(func)) {
    serial_queue_task.svc = svc;
    serial_queue_task.del = del;
    serial_queue_task.arg = this;
  }
  template <class In> SerialQueueTaskNode &operator=(In &func) {
    new (&m_func) Functor(std::forward<In>(func));
    return *this;
  }
  Functor m_func;
  ntt_sq_task serial_queue_task;
};

template <class Functor> struct SerialQueueTaskNodePool {
  SerialQueueTaskNodePool() { m_node.next = NULL; }
  SerialQueueTaskNode<Functor> *get() {
    if (auto node = alloc()) {
      ntt_node *t1 = ntt_container_of(node, ntt_node, node);
      ntt_sq_task *t2 = ntt_container_of(t1, ntt_sq_task, node);
      return ntt_container_of(t2, SerialQueueTaskNode<Functor>,
                              serial_queue_task);
    };
    return nullptr;
  }

  struct cds_wfcq_node *alloc() {
    if (m_node.next != NULL) {
      auto tmp = m_node.next;
      m_node.next = m_node.next->next;
      if (m_tail == tmp) {
        m_tail = &m_node;
      }
      return tmp;
    }
    return nullptr;
  }
  void free(SerialQueueTaskNode<Functor> *node) {
    m_tail->next = &node->serial_queue_task.node.node;
    m_tail = &node->serial_queue_task.node.node;
    cnt += 1;
  };
  struct cds_wfcq_node m_node;
  struct cds_wfcq_node *m_tail = &m_node;
  std::size_t cnt = 0;
  ntt_mpsc_queue queue;
};

template <class Functor> SerialQueueTaskNodePool<Functor> &get_pool() {
  thread_local SerialQueueTaskNodePool<Functor> pool;
  return pool;
}

template <class Function>
/* static */ void SerialQueueTaskNode<Function>::del(void *arg) {
  auto task_node = static_cast<SerialQueueTaskNode *>(arg);
  get_pool<Function>().free(task_node);
}

template <class Functor> ntt_sq_task *make_sq_task_cached(Functor &&functor) {
  if (auto ptr = get_pool<Functor>().get()) {
    *ptr = functor;
    return &ptr->serial_queue_task;
  }
  auto node = new SerialQueueTaskNode<std::decay_t<Functor>>(
      std::forward<Functor>(functor));
  return &node->serial_queue_task;
}

template <class Functor> ntt_sq_task *make_sq_task(Functor &&functor) {
  auto node = new SerialQueueTaskNode<std::decay_t<Functor>>(
      std::forward<Functor>(functor));
  return &node->serial_queue_task;
}

TEST(ntt, task_queue_dep) {
  static constexpr std::size_t g_width = 8;
  std::promise<void> p;
  auto f = p.get_future();

  ntt_event_queue event_queue;
  ntt_event_queue_init(&event_queue);

  ntt_thread_pool_config config{};
  config.width = g_width;
  config.svc_arg = &event_queue;
  config.svc_cb = [](void *arg) {
    auto *event_queue = static_cast<ntt_event_queue *>(arg);
    struct ntt_event *event = NULL;
    while ((event = ntt_event_queue_pop(event_queue))) {
      event->svc_cb(event->svc_arg);
    }
  };
  config.complete_arg = &p;
  config.complete_cb = [](void *arg) {
    static_cast<std::promise<void> *>(arg)->set_value();
  };
  auto *thread_pool = ntt_thread_pool_create_from_config(&config);
  ntt_thread_pool_release(thread_pool);

  static constexpr std::size_t g_expected_cnt = 1'000;
  std::atomic<std::size_t> cnt = 0;
  std::promise<void> done;
  auto future = done.get_future();
  ntt_event_queue_push(&event_queue, make_event([&] {
    for (std::size_t i = 0; i < g_expected_cnt; ++i) {
      ntt_event_queue_push(&event_queue, make_event([&] {
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
  ntt_event_queue_stop(&event_queue);
  f.wait();
  ntt_event_queue_destroy(&event_queue);
}

static constexpr std::size_t g_cnt = 100'000;
static constexpr std::size_t n = 20;
static constexpr std::size_t m = 1000;
static constexpr std::size_t k = 4;
static constexpr std::chrono::microseconds g_wakeup{10};
static constexpr std::chrono::microseconds g_wakeup_noise{2};
static constexpr std::chrono::microseconds g_recv_work{5};
static constexpr std::chrono::microseconds g_recv_noise{2};
static constexpr std::chrono::microseconds g_proc_work{10};
static constexpr std::chrono::microseconds g_proc_noise{2};

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

  struct ctx {
    std::vector<std::size_t> fn = std::vector<std::size_t>(g_cnt);
    std::vector<std::size_t> fm = std::vector<std::size_t>(g_cnt);
    std::vector<std::size_t> fk = std::vector<std::size_t>(g_cnt);

    std::vector<std::chrono::microseconds> fnw{g_cnt};
    std::vector<std::chrono::microseconds> fmw{g_cnt};
    std::vector<std::chrono::microseconds> fkw{g_cnt};

    std::vector<ntt_task_queue *> recv{n};
    std::vector<ntt_task_queue *> proc{m};
    std::vector<ntt_task_queue *> send{k};

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

  ntt_work_loop_config config{};
  config.width = g_width;
  config.complete_arg = &p;
  config.complete_cb = [](void *arg) {
    static_cast<std::promise<void> *>(arg)->set_value();
  };

  auto *work_loop = ntt_work_loop_create_from_config(&config);

  for (auto &group : {&ctx.recv, &ctx.proc, &ctx.send}) {
    for (auto &queue : *group) {
      queue = ntt_task_queue_create(work_loop);
    }
  }

  ntt_work_loop_release(work_loop);

  for (std::size_t i = 0; i < g_cnt; ++i) {
    int x = i;
    std::this_thread::sleep_for(ctx.fnw[x]);
    ctx.in[x] = std::chrono::high_resolution_clock::now();
    ntt_task_queue_dispatch(
        ctx.recv[ctx.fn[x]], make_sq_task_cached([ctx = &ctx, x = i] mutable {
          // ctx->in[x] = std::chrono::high_resolution_clock::now();
          // std::this_thread::sleep_for(ctx->fmw[x]);
          // we are in recv queue
          ntt_task_queue_dispatch(
              ctx->proc[ctx->fm[x]], make_sq_task_cached([ctx, x = x] mutable {
                // std::this_thread::sleep_for(ctx->fkw[x]);
                // we are in proc queue
                ntt_task_queue_dispatch(
                    ctx->send[ctx->fk[x]],
                    make_sq_task_cached([ctx, x = x] mutable {
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

  for (auto &group : {&ctx.recv, &ctx.proc, &ctx.send}) {
    for (auto &queue : *group) {
      ntt_task_queue_release(queue);
    }
  }

  f.wait();

  std::vector<std::chrono::high_resolution_clock::duration> res(g_cnt);
  for (std::size_t i = 0; i < g_cnt; ++i) {
    res[i] = ctx.out[i] - ctx.in[i];
  }
  std::sort(res.begin(), res.end());
  std::cout << "50%% " << res[static_cast<int>(res.size() * 0.5)].count()
            << std::endl;
  std::cout << "80%% " << res[static_cast<int>(res.size() * 0.8)].count()
            << std::endl;
  std::cout << "95%% " << res[static_cast<int>(res.size() * 0.95)].count()
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
      // ctx->in[x] = std::chrono::high_resolution_clock::now();
      // std::this_thread::sleep_for(ctx->fmw[x]);
      // we are in recv queue
      boost::asio::dispatch(ctx->proc[ctx->fm[x]], [ctx, x = x] {
        // std::this_thread::sleep_for(ctx->fkw[x]);
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
  std::cout << "50%% " << res[static_cast<int>(res.size() * 0.5)].count()
            << std::endl;
  std::cout << "80%% " << res[static_cast<int>(res.size() * 0.8)].count()
            << std::endl;
  std::cout << "95%% " << res[static_cast<int>(res.size() * 0.95)].count()
            << std::endl;
}
