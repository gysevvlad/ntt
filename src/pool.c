#include "ntt/pool.h"
#include "ntt/impl/list.h"
#include "ntt/impl/ntt_task_cache.h"
#include "ntt/impl/task.h"
#include "task_list.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/syscall.h>

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct ntt_thread_slot_t {
  ntt_pool_t *pool;
  pthread_t thread_id;
  int action_cnt;
  ntt_task_list_t tasks;
  pthread_spinlock_t tasks_lock;
  ntt_task_node_t stop_task;
} ntt_thread_slot_t;

typedef struct ntt_thread_slot_t ntt_thread_t;

struct ntt_pool {
  atomic_size_t internal_refs;
  atomic_size_t external_refs;

  size_t thread_cnt;
  int epollfd;
  int eventfd;

  ntt_task_list_t tasks;
  int task_awaiters;
  pthread_spinlock_t tasks_lock;

  ntt_task_cache_t *task_cache;

  ntt_thread_slot_t threads[];
};

#define SIGNTTACTION SIGRTMIN + 16

void ntt_pool_destroy(ntt_pool_t *self) {
  pthread_spin_destroy(&self->tasks_lock);

  int rc = epoll_ctl(self->epollfd, EPOLL_CTL_DEL, self->eventfd, NULL);
  assert(rc == 0 && "ntt pool failed to del internal event fd");

  rc = close(self->epollfd);
  assert(rc == 0 && "ntt pool failed to close internal event fd");

  rc = close(self->eventfd);
  assert(rc == 0 && "ntt pool failed to close epoll fd");

  // TODO: support external stop slot
  size_t i;
  for (i = 0; i < self->thread_cnt; ++i) {
    pthread_detach(self->threads[i].thread_id);
  }

  ntt_task_cache_destroy(self->task_cache);

  free(self);
}

void ntt_pool_release_internal(ntt_pool_t *self) {
  assert(atomic_load_explicit(&self->external_refs, memory_order_relaxed) ==
             0 &&
         "ntt pool has unexpected external refs");
  size_t prev = atomic_fetch_sub(&self->internal_refs, 1);
  assert(prev > 0 && "ntt pool has unexpected internal refs");
  if (prev == 1) {
    ntt_pool_destroy(self);
  }
}

void *ntt_thread_svc(void *arg) {
  int active = 1;
  ntt_thread_t *thread = arg;
  sigset_t sigset;
  sigemptyset(&sigset);
  pthread_sigmask(0, NULL, &sigset);
  sigdelset(&sigset, SIGNTTACTION);

  struct epoll_event ev;
  while (active) {
    int rc = epoll_pwait(thread->pool->epollfd, &ev, 1, -1, &sigset);
    if (rc == -1) {
      if (errno != EINTR) {
        abort(); // shit happens
      }
      if (thread->action_cnt == 0) {
        continue; // unknown interrupt, skip
      }
      int last;
      do {
        ntt_task_t *task = ntt_task_list_front(&thread->tasks);
        ntt_do_task_inl(task);
        pthread_spin_lock(&thread->tasks_lock);
        ntt_task_list_pop(&thread->tasks, &last);
        pthread_spin_unlock(&thread->tasks_lock);
        ntt_free_task_inl(task);
      } while (!last);
      thread->action_cnt = 0;
      continue;
    }
    if (ev.data.fd == thread->pool->eventfd) {
      // internal queue wakeup
      int last;
      ntt_task_t *task = NULL;
      pthread_spin_lock(&thread->pool->tasks_lock);
      while ((task = ntt_task_list_pop(&thread->pool->tasks, &last)) != NULL) {
        pthread_spin_unlock(&thread->pool->tasks_lock);
        if (ntt_task_is_null(task)) {
          assert(atomic_load(&thread->pool->external_refs) == 0 &&
                 "ntt thread got unexpected null task");
          active = 0;
          break; // stop loop
        }
        ntt_do_task_inl(task);
        ntt_free_task_inl(task);
        pthread_spin_lock(&thread->pool->tasks_lock);
      }
      thread->pool->task_awaiters += 1;
      pthread_spin_unlock(&thread->pool->tasks_lock);
    }
  }
  ntt_pool_release_internal(thread->pool);
  return NULL;
}

void handler(int signo, siginfo_t *info, void *context) {
  ntt_thread_slot_t *thread_slot = info->si_value.sival_ptr;
  thread_slot->action_cnt += 1;
  assert(signo == SIGNTTACTION);
}

void ntt_pool_post_task(ntt_pool_t *self, ntt_task_t *task) {
  int need_wakeup = 0;
  pthread_spin_lock(&self->tasks_lock);
  int first;
  ntt_task_list_push(&self->tasks, task, &first);
  if (self->task_awaiters > 0) {
    self->task_awaiters -= 1;
    need_wakeup = 1;
  }
  pthread_spin_unlock(&self->tasks_lock);
  if (need_wakeup) {
    eventfd_write(self->eventfd, 1);
  }
}

ntt_pool_t *ntt_pool_create(unsigned short width) {
  int rc;
  int i;

  ntt_pool_t *self =
      malloc(sizeof(ntt_pool_t) + sizeof(ntt_thread_slot_t) * width);
  assert(self != NULL);

  self->external_refs = 1;
  self->internal_refs = width;

  self->thread_cnt = width;

  self->eventfd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE);
  assert(self->eventfd != -1);

  self->epollfd = epoll_create1(EPOLL_CLOEXEC);
  assert(self->epollfd != -1);

  struct epoll_event event;
  event.data.fd = self->eventfd;
  event.events = EPOLLIN | EPOLLET;
  rc = epoll_ctl(self->epollfd, EPOLL_CTL_ADD, self->eventfd, &event);
  assert(rc == 0);

  // init combined queue
  ntt_task_list_init(&self->tasks);
  self->task_awaiters = width;
  pthread_spin_init(&self->tasks_lock, PTHREAD_PROCESS_PRIVATE);

  // init task cache
  self->task_cache = ntt_task_cache_create();

  // add signal handler for SIGNTTACTION
  struct sigaction action = {0};
  action.sa_flags = SA_SIGINFO;
  action.sa_sigaction = &handler;
  rc = sigaction(SIGNTTACTION, &action, NULL);
  assert(rc != -1);

  // block SIGNTTACTION
  sigset_t sigset;
  rc = sigemptyset(&sigset);
  assert(rc != -1);
  rc = sigaddset(&sigset, SIGNTTACTION);
  assert(rc != -1);
  sigset_t origin_sigset;
  rc = sigprocmask(SIG_BLOCK, &sigset, &origin_sigset);
  assert(rc != -1);

  for (i = 0; i < width; ++i) {
    self->threads[i].pool = self;
    self->threads[i].action_cnt = 0;
    ntt_task_list_init(&self->threads[i].tasks);
    pthread_spin_init(&self->threads[i].tasks_lock, PTHREAD_PROCESS_PRIVATE);
    rc = pthread_create(&self->threads[i].thread_id, NULL, ntt_thread_svc,
                        &self->threads[i]);
    assert(rc == 0);
  }

  // return back original sigset
  rc = sigprocmask(SIG_SETMASK, &origin_sigset, NULL);
  assert(rc != -1);

  return self;
}

void ntt_thread_push_task(ntt_thread_t *thread, ntt_task_t *task) {
  int first;
  pthread_spin_lock(&thread->tasks_lock);
  ntt_task_list_push(&thread->tasks, task, &first);
  pthread_spin_unlock(&thread->tasks_lock);
  if (first) {
    union sigval sigval;
    sigval.sival_ptr = thread;
    int rc;
    do {
      rc = pthread_sigqueue(thread->thread_id, SIGNTTACTION, sigval);
    } while (rc == EAGAIN);
    assert(rc == 0);
  }
}

void ntt_pool_send_task(ntt_pool_t *pool, unsigned short idx,
                         ntt_task_t *task) {
  ntt_thread_push_task(&pool->threads[idx], task);
}

ntt_task_t *ntt_pool_alloc_task(ntt_pool_t *pool, ntt_task_cb_t *task_cb) {
  return ntt_task_cache_alloc_task(pool->task_cache, task_cb);
}

void ntt_pool_stop(ntt_pool_t *self) {
  unsigned int i;
  for (i = 0; i < self->thread_cnt; ++i) {
    ntt_pool_post_task(self, &self->threads[i].stop_task.payload);
  }
}

void ntt_pool_acquire(ntt_pool_t *self) {
  size_t prev = atomic_fetch_add(&self->external_refs, 1);
  assert(prev > 0 && "ntt pool already deleted");
}

void ntt_pool_release(ntt_pool_t *self) {
  size_t prev = atomic_fetch_sub(&self->external_refs, 1);
  assert(prev > 0 && "ntt pool already deleted");
  if (prev == 1) {
    ntt_pool_stop(self);
  }
}
