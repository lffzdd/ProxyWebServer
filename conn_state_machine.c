#include "conn_state_machine.h"
#include"net_utils.h"
#include"http_util.h"
#include"http_proxy.h"
#include<sys/epoll.h>
#include<malloc.h>

int add_conn_pair(int epfd, int listen_fd) {
    int client_fd = acceptClientfd(listen_fd);
    int server_fd;

    http_request_t req = { 0 };
    parseHttpRequest(client_fd, &req);
    if (req.method == CONNECT)
        server_fd = openConnectfd(req.host, "443");
    else
        server_fd = openConnectfd(req.host, "80");

    conn_t* conn = malloc(sizeof(conn_t));
    conn->client_fd = client_fd;
    conn->server_fd = server_fd;
    conn->state = CONN_READING;

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = conn;

    epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev);
    epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev);
}
int handle_connection_state(conn_t* conn, int ready_fd, int epfd) {
    switch (conn->state) {
        case  CONN_INIT:
            add_conn_pair(epfd, ready_fd);
            return 0;
        case CONN_READING:
        
        default:
            break;
    }
    return 0;
}

