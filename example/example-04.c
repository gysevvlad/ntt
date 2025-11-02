#include <ares.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

static void dns_callback(void* arg, int status, int timeouts, struct hostent* host)
{
    if (status != ARES_SUCCESS) {
        printf("DNS failed: %s\n", ares_strerror(status));
        return;
    }
    char ip[INET6_ADDRSTRLEN];
    inet_ntop(host->h_addrtype, host->h_addr_list[0], ip, sizeof(ip));
    printf("Resolved: %s -> %s\n", host->h_name, ip);
}

int main()
{
    ares_channel channel;
    ares_library_init(ARES_LIB_INIT_ALL);
    ares_init(&channel);

    ares_gethostbyname(channel, "example.com", AF_INET, dns_callback, NULL);

    int epfd = epoll_create1(0);
    while (1) {
        int nfds;
        fd_set read_fds, write_fds;
        int nfds_ares, max_fd = 0;
        struct epoll_event ev;

        int rfd, wfd;
        struct timeval *tvp, tv;

        ares_fds(channel, &read_fds, &write_fds);
        nfds_ares = 0;
        for (int fd = 0; fd < FD_SETSIZE; fd++) {
            if (FD_ISSET(fd, &read_fds) || FD_ISSET(fd, &write_fds)) {
                ev.events = 0;
                if (FD_ISSET(fd, &read_fds))
                    ev.events |= EPOLLIN;
                if (FD_ISSET(fd, &write_fds))
                    ev.events |= EPOLLOUT;
                ev.data.fd = fd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
                if (fd > nfds_ares)
                    nfds_ares = fd;
            }
        }

        tvp  = ares_timeout(channel, NULL, &tv);
        nfds = epoll_wait(epfd, &ev, 1, tvp ? tvp->tv_sec * 1000 : 1000);
        if (nfds > 0) {
            ares_process(channel, &read_fds, &write_fds);
        } else {
            ares_process_fd(channel, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
        }

        // Break condition if all queries done:
        if (ares_fds(channel, &read_fds, &write_fds) == 0)
            break;
    }

    ares_destroy(channel);
    ares_library_cleanup();
    close(epfd);
    return 0;
}
