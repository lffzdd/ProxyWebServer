#include "config.h"
#include "http_proxy.h"
#include "http_util.h"
#include "net_utils.h"
#include "rio.h"
#include <fcntl.h>
#include <sys/epoll.h>
#include<stdlib.h>
#include<netdb.h>
#include"conn_state_machine.h"

int main() {
    // 监听客户端
    int listen_fd = openListenfd(PORT);
    if (listen_fd < 0) {
        perror("openListenfd");
        exit(1);
    }

    printf("代理服务器启动，监听端口: %s，listen_fd = %d\n", PORT, listen_fd);

    if (listen(listen_fd, EPOLL_LEN) < 0) {
        perror("listen");
        exit(1);
    }

    int epfd = epoll_create1(0);
    if (epfd < 0) {
        perror("epoll_create1");
        exit(1);
    }


    struct epoll_event events[EPOLL_LEN * 2], ev;
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) < 0) {
        perror("epoll_ctl listenfd");
        exit(1);
    }


    while (1) {
        int nready = epoll_wait(epfd, events, EPOLL_LEN * 2, -1);
        printf("epoll_wait 返回 %d 个事件\n", nready);

        for (int i = 0; i < nready; i++) {
            int ready_fd = events[i].data.fd;
            printf("处理 fd = %d 的事件\n", ready_fd);

            if (ready_fd == listen_fd) { // 新连接到来
                printf("检测到新连接\n");
                add_client_to_epoll(epfd, ready_fd);
            }
            else { // 已有连接
                conn_t* conn = events[i].data.ptr;
                if (conn) {
                    handle_connection_state(conn, ready_fd, epfd);
                }
            }
        }
    }

    close(epfd);
    close(listen_fd);

    return 0;
}
