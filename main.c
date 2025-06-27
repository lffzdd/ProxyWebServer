#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/epoll.h>

#include "config.h"
#include "conn_state_machine.h"
#include "http_proxy.h"
#include "http_util.h"
#include "net_utils.h"
#include "rio.h"
#include "sys_wrap.h"

int main() {
    // 监听客户端
    int listen_fd = openListenfd(PORT);
    if (listen_fd < 0) {
        perror("openListenfd");
        exit(1);
    }

    printf("代理服务器启动，监听端口: %s，listen_fd = %d\n", PORT, listen_fd);

    Listen(listen_fd, EPOLL_LEN);

    int epfd = epoll_create1(0);
    if (epfd < 0) {
        perror("epoll_create1");
        exit(1);
    }

    struct epoll_event events[EPOLL_LEN * 2];

    struct epoll_event ev = { .events = EPOLLIN, .data.ptr = NULL };
    Epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);

    while (1) {
        int nready = epoll_wait(epfd, events, EPOLL_LEN * 2, -1);
        printf("epoll_wait 返回 %d 个事件\n", nready);

        for (int i = 0; i < nready; i++) {

            fd_event_t* fd_event = events[i].data.ptr;

            if (fd_event == NULL) { // 新连接到来
                printf("检测到新连接\n");
                add_client_to_epoll(epfd, listen_fd);
            }
            else { // 已有连接
                if (handle_connection_state(fd_event, epfd) == 1)
                    handle_connection_state(fd_event, epfd);
            }
        }
    }

    close(epfd);
    close(listen_fd);

    return 0;
}
