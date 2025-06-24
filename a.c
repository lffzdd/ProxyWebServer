#include "config.h"
#include "http_proxy.h"
#include "http_util.h"
#include "net_utils.h"
#include "rio.h"
#include <fcntl.h>
#include <sys/epoll.h>

#define EPOLL_LEN 1024
int main() {
    // 监听客户端
    int listen_fd = openListenfd(PORT);
    int epfd = epoll_create1(0);
    struct epoll_event events[EPOLL_LEN], ev;
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);

    

    while (1) {
        int nready = epoll_wait(epfd, events, EPOLL_LEN, -1);
        for (int i = 0; i < nready; i++) {
            int fd = events[i].data.fd;

            if (fd == listen_fd) { // 如果listen到了新的client
                int client_fd = acceptClientfd(listen_fd);

                ev.events = EPOLLIN;
                ev.data.fd = client_fd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev);

                // 第一次的时候要为client连接server
                http_request_t req = { 0 };
                parseHttpRequest(fd, &req);

                int server_fd;
                if (req.method == CONNECT)
                    server_fd = openConnectfd(req.host, "443");
                else
                    server_fd = openConnectfd(req.host, "80");

                ev.events = EPOLLIN;
                ev.data.fd = server_fd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev);

                add_fd_pair(&fd_map, client_fd, server_fd);
            } else { // 如果是已存在的client或server
                int peer_fd = (get_peer_fd(fd_map, fd)).fd;

                rio_t rio;
                char buf[MAXBUF];
                rio_readinitb(&rio, fd);
                int n;
                while ((n = rio_readlineb(&rio, buf, sizeof(buf))) > 0) {
                    rio_written(peer_fd, buf, n);
                }
            }
        }
    }

    return 0;
}
