#include "ntt/impl/tcp_source.h"
#include "ntt/impl/pool.h"
#include "ntt/impl/socket.h"
#include "ntt/pool.h"
#include "ntt/tcp_source.h"
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>

enum NTT_SOURCE_STATE_MASKS {
    // clang-format off
    NTT_PENDING_IN_SOURCE_STATE  = 0b0000001,
    NTT_PENDING_OUT_SOURCE_STATE = 0b0000010,
    NTT_PENDING_ERR_SOURCE_STATE = 0b0000100,
    NTT_PENDING_HUP_SOURCE_STATE = 0b0001000,
    NTT_PENDING_ANY_SOURCE_STATE = 0b0001111,
    NTT_PENDING_END_SOURCE_STATE = NTT_PENDING_ERR_SOURCE_STATE | NTT_PENDING_HUP_SOURCE_STATE,
    NTT_BUSY_SOURCE_STATE        = 0b1000000,
    // clang-format on
};

static uint_fast32_t ntt_map_epoll_events_to_source_state(uint32_t events)
{
    size_t v = 0;
    if (events & EPOLLIN) {
        v = v | NTT_PENDING_IN_SOURCE_STATE;
    }
    if (events & EPOLLOUT) {
        v = v | NTT_PENDING_OUT_SOURCE_STATE;
    }
    if (events & EPOLLERR) {
        v = v | NTT_PENDING_ERR_SOURCE_STATE;
    }
    if (events & EPOLLHUP) {
        v = v | NTT_PENDING_HUP_SOURCE_STATE;
    }
    if (events & EPOLLRDHUP) {
        // ignore EPOLLRDHUP
    }
    if (events & EPOLLPRI) {
        // ignore EPOLLPRI
    }
    return v;
}

void ntt_tcp_source_r_svc(void* ctx, uint32_t events)
{
    ntt_tcp_source_t* self = ctx;

    // we can handle following events:
    //  EPOLLHUP - connection closed            // connection normally closed, disable writing, stop read side
    //  EPOLLERR - connection closed with error // get sock error and stop, disable writing, stop reading
    //  EPOLLIN - any data available for read   // data ready
    //  EPOLLRDHUP - no read data anymore       // read side closed, put shutdown to write queue

    uint_fast32_t state = atomic_load_explicit(&self->state, memory_order_relaxed);

    do {
        if (state & NTT_BUSY_SOURCE_STATE) {
            // other thread working with fd, set pending events
            uint_fast32_t next_state = state | ntt_map_epoll_events_to_source_state(events);
            if (atomic_compare_exchange_weak(&self->state, &state, next_state)) {
                // notification about pending event setted, returns
                return;
            }
        } else {
            // we first thread on fd, try to take lock
            uint_fast32_t next_state = state | NTT_BUSY_SOURCE_STATE;
            if (atomic_compare_exchange_weak(&self->state, &state, next_state)) {
                // lock taken, continue
                break;
            }
        }
    } while (1);

    do {
        int rc = read(self->r_sock, self->in_buf_data, self->in_buf_len);
        if (rc == -1) {
            if (rc == EINTR) {
                continue;
            }
            if (rc == EAGAIN || rc == EWOULDBLOCK) {
                uint_fast32_t state = atomic_load_explicit(&self->state, memory_order_relaxed);
                do {
                    if (state & NTT_PENDING_ANY_SOURCE_STATE) {
                        uint_fast32_t next_state = state & ~NTT_PENDING_ANY_SOURCE_STATE;

                        if (atomic_compare_exchange_weak(&self->state, &state, next_state)) {
                            continue;
                        }
                    } else {
                        // try release lock

                        uint_fast32_t next_state = state & ~NTT_BUSY_SOURCE_STATE;

                        if (atomic_compare_exchange_weak(&self->state, &state, next_state)) {
                            return;
                        }
                    }
                } while (1);

                // TODO: ...
                assert(0);
                return;
            }
            // error condition
            // TODO: ...
            assert(0);
            return;
        }
        if (rc == 0) {
            // read done
            // TODO: ...
            assert(0);
            return;
        }
        self->in_buf_data += rc;
        self->in_buf_len -= rc;
    } while (self->in_buf_len > 0);
}

void ntt_tcp_source_svc(void* ctx, uint32_t events)
{
    // TODO: ...
    fprintf(stderr, "ooooooooooooops!");
}

void ntt_tcp_source_construct(
    ntt_tcp_source_t* self,
    ntt_pool_t* pool,
    ntt_sockaddr_t* sockaddr)
{
    self->pool = pool;
    self->dst_sockaddr = *sockaddr;
    self->r_sock = -1;
    self->r_event.ctx = self;
    self->w_event.ctx = self;
    pthread_mutex_init(&self->w_mutex, NULL);

    ntt_pool_acquire(self->pool);
}

void ntt_tcp_source_destruct(
    ntt_tcp_source_t* self)
{
    pthread_mutex_destroy(&self->w_mutex);
    ntt_pool_release(self->pool);
}

ntt_tcp_source_t* ntt_tcp_source_create(
    ntt_pool_t* pool)
{
    ntt_tcp_source_t* self = malloc(sizeof(ntt_tcp_source_t));
    self->pool = ntt_pool_acquire(pool);
}

void ntt_tcp_source_start(
    ntt_tcp_source_t* self,
    int connected_socket)
{
    int ec = 0;
    int rc = 0;

    self->r_sock = connected_socket;

    rc = dup(self->r_sock);

    if (rc == -1) {
        ec = errno;
        ntt_socket_close(&self->r_sock);
        fprintf(stderr, "[ntt_tcp_source] dup(...) failed: %s (%i)", strerror(ec), ec);
        abort(); // TODO: fallpath
    }

    self->w_sock = rc;

    rc = fcntl(self->w_sock, F_SETFD, FD_CLOEXEC);

    if (rc == -1) {
        ec = errno;
        ntt_socket_close(&self->r_sock);
        fprintf(stderr, "[ntt_tcp_source] fcntl(...) failed: %s (%i)", strerror(ec), ec);
        abort(); // TODO: fallpath
    }

    self->r_event.cb = ntt_tcp_source_r_svc;

    struct epoll_event event;
    event.data.ptr = &self->r_event;
    event.events = EPOLLIN | EPOLLET;

    rc = epoll_ctl(self->pool->epoll_fd, EPOLL_CTL_ADD, self->r_sock, &event);

    if (rc == -1) {
        ec = errno;
        ntt_socket_close(&self->r_sock);
        ntt_socket_close(&self->w_sock);
        fprintf(stderr, "[ntt_tcp_source] epoll_ctl(...) failed: %s (%i)", strerror(ec), ec);
        abort(); // TODO: fallpath
    }
}

void ntt_tcp_source_stop_svc(void* ctx)
{
    ntt_tcp_source_t* self = ctx;

    ntt_socket_close(&self->r_sock);

    fprintf(stderr, "[ntt_tcp_source] stopped");
}

void ntt_tcp_source_stop(
    ntt_tcp_source_t* self)
{
    int rc = epoll_ctl(self->pool->epoll_fd, EPOLL_CTL_DEL, self->r_sock, NULL);

    if (rc == -1) {
        int ec = errno;
        // TODO: handle error, do stop procedure
        printf("failed to del accept source from epoll: %s (%i)", strerror(ec), ec);
        abort();
    }

    ntt_task_t* task = ntt_make_task(ntt_tcp_source_stop_svc, NULL);
    *(ntt_tcp_source_t**)task = self;
    ntt_pool_post_barrier_task(self->pool, task);
}

void ntt_tcp_source_delete(
    ntt_tcp_source_t* self)
{
    ntt_tcp_source_destruct(self);
    free(self);
}

void ntt_tcp_source_send(
    ntt_tcp_source_t* self,
    uint8_t* data,
    size_t len)
{
    pthread_mutex_lock(&self->w_mutex);
    do {
        int rc = write(self->r_sock, data, len);
        if (rc == -1) {
            int ec = errno;
            if (ec == EINTR) {
                // try again
                continue;
            }
            // TODO: stop connection with ec error
            abort();
            pthread_mutex_unlock(&self->w_mutex);
            return;
        }
        len -= rc;
        data += rc;
    } while (len != 0);
    pthread_mutex_unlock(&self->w_mutex);
}
