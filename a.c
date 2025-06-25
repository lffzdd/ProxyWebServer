#include "config.h"
#include "http_proxy.h"
#include "http_util.h"
#include "net_utils.h"
#include "rio.h"
#include <fcntl.h>
#include <sys/epoll.h>
#include<stdlib.h>
#include"conn_state_machine.h"

#define EPOLL_LEN 1024
int main() {
    // 监听客户端
    int listen_fd = openListenfd(PORT);
    int epfd = epoll_create1(0);

    struct epoll_event events[EPOLL_LEN], ev;

    conn_t conn = { 0 };
    conn.state = CONN_INIT;

    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    ev.data.ptr = &conn;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);


    while (1) {
        int nready = epoll_wait(epfd, events, EPOLL_LEN, -1);
        for (int i = 0; i < nready; i++) {
            int ready_fd = events[i].data.fd;
            conn_t* conn = events[i].data.ptr;
            handle_connection_state(conn, ready_fd);
        }
    }

    return 0;
}
