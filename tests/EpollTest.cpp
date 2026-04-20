#include <gtest/gtest.h>

#include <stdexcept>
#include <sys/epoll.h>
#include <system_error>

namespace os {

static int make_epoll()
{
    int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd == -1) {
        throw std::system_error { epoll_fd, std::system_category() };
    }
    return epoll_fd;
}

} // namespace os

TEST(EpollTest, DISABLED_WaitOnEmptyEpollInstance)
{
    auto fd = os::make_epoll();

    std::array<struct epoll_event, 1> events;
    epoll_wait(fd, events.data(), events.size(), -1);
}
