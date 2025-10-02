#include "ntt/event.h"
#include "ntt/defs.h"
#include "ntt/loop.h"
#include "ntt/ntt.h"

#include <cerrno>
#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <sys/eventfd.h>
#include <thread>
#include <unistd.h>

TEST(NttEvent, Common)
{
    int efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    ASSERT_NE(efd, -1);

    static std::size_t g_expected_values = 12666;

    struct Event {
        int fd            = -1;
        std::uint64_t val = 0;
        std::promise<void> done;
    } event;

    event.fd = efd;

    static ntt_event_vtbl_t g_event_vtbl = {
        .name                 = "test_event",
        .ntt_event_svc_cb     = [](ntt_event_t* self, void* ctx, ntt_interest_t interest) {
            auto* event = static_cast<Event*>(ctx);

            for (;;) {
                uint64_t lval = 0;

                int rc = eventfd_read(event->fd, &lval);

                if (ntt_unlikely(rc != 0)) {
                    if (ntt_unlikely(errno == EINTR)) {
                        continue; // try again
                    }
                    if (ntt_unlikely(errno != EWOULDBLOCK)) {
                        // something fatal happening on fd
                        assert(false);
                    }
                    break; // read done
                }

                event->val += lval;

                if (event->val == g_expected_values) {
                    event->done.set_value();
                }
            } },
        .ntt_event_stopped_cb = [](ntt_event_t* self, void* ctx, ntt_ec_t ec) { std::cout << "event stopped\n"; }
    };

    auto* e = ntt_event_create(&g_event_vtbl, &event, efd, NTT_INTEREST_READABLE);

    static ntt_loop_vptr_t loop_vtbl {
        .name    = "test_loop",
        .started = +[](ntt_loop_t* loop, void* ctx) {
            auto* e = static_cast<ntt_event_t*>(ctx);
            ntt_event_start(e, loop);
        },
    };

    std::jthread thread { [&] {
        for (std::size_t i = 0; i < g_expected_values; ++i) {
            eventfd_write(event.fd, 1);
            std::this_thread::sleep_for(std::chrono::microseconds { 20 });
        }
        event.done.get_future().wait();
        ntt_event_cancel(e);
    } };

    ntt_loop_svc(&loop_vtbl, e, 8);
    std::cout << "loop stopped\n";
    ntt_event_delete(e);
    close(event.fd);
}
