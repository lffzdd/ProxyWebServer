#define _POSIX_C_SOURCE 200122L
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>

#include "config.h"
#include "net_utils.h"

#include<sys/epoll.h>

int main(int argc, char const* argv[]) {

    int listen_fd = openListenfd(PORT);
    listen(listen_fd, 5);

    int epfd = epoll_create1(0);

    struct epoll_event events[1024];
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);

    while (1) {
        int nready = epoll_wait(epfd, events, 1024, -1);
        for (int i = 0; i < nready; i++) {
            int fd = events[i].data.fd;
            if (fd == listen_fd) {
                int client_fd = acceptClientfd(listen_fd);

                ev.events = EPOLLIN;
                ev.data.fd = client_fd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev);
            }
            else {
                respondClient(fd);
                epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                close(fd);
            }
        }

    }
    close(listen_fd);

    return 0;
}
