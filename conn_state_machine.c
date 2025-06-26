#include "conn_state_machine.h"
#include "http_util.h"
#include "net_utils.h"
#include "sys_wrap.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

int add_client_to_epoll(int epfd, int listen_fd) {
    int client_fd = acceptClientfd(listen_fd);

    conn_t* conn = Malloc(sizeof(conn_t));

    memset(conn, 0, sizeof(conn_t));
    conn->client_fd = client_fd;
    conn->state = CONN_INIT;

    fd_event_t* fd_event = Malloc(sizeof(fd_event_t));
    memset(fd_event, 0, sizeof(fd_event_t));
    fd_event->conn = conn;
    fd_event->is_client = 1;

    conn->client_event = fd_event;

    struct epoll_event ev = { .events = EPOLLIN, .data.ptr = fd_event };
    Epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev);

    return 0;
}

int add_server_to_epoll(int epfd, conn_t* conn) {
    int client_fd = conn->client_fd;
    int server_fd;

    http_request_t req = { 0 };
    if (parseHttpRequest(client_fd, &req) != 0) {
        perror("add_server_to_epoll->parseHttpRequest");
        return 1;
    }

    if (req.method == CONNECT)
        server_fd = openConnectfd(req.host, "443");
    else
        server_fd = openConnectfd(req.host, "80");

    if (server_fd <= 0) {
        perror("add_server_to_epoll->openConnectfd");
        return 1;
    }

    conn->server_fd = server_fd;

    fd_event_t* fd_event = Malloc(sizeof(fd_event_t));
    fd_event->conn = conn;
    fd_event->is_client = 0;

    conn->server_event = fd_event;

    struct epoll_event ev = { .events = EPOLLIN, .data.ptr = fd_event };
    Epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev);

    return 0;
}

int handle_connection_c2s_forwarding(conn_t* conn, conn_stat_t eof_state, int epfd) {

    // 一.首先尝试还没写完的数据
    while (conn->buf_c2s_len > conn->buf_c2s_sent) {
        int n = write(conn->server_fd, conn->buf_c2s + conn->buf_c2s_sent, conn->buf_c2s_len - conn->buf_c2s_sent);

        if (n < 0) { // 没写进去,要么阻塞要么出错
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 阻塞,注册 EPOLLOUT事件,让它写完
                struct epoll_event ev = { .events = EPOLLIN | EPOLLOUT,.data.ptr = conn->server_event };
                Epoll_ctl(epfd, EPOLL_CTL_MOD, conn->server_fd, &ev);
                return 0;
            } else {
                perror("write to server");
                return 1;
            }
        }
        conn->buf_c2s_sent += n;
    }

    // 如果所有数据都被写完了,取消监听,清空缓冲区状态

    struct epoll_event ev = { .events = EPOLLIN ,.data.ptr = conn->server_event };
    Epoll_ctl(epfd, EPOLL_CTL_MOD, conn->server_fd, &ev);

    conn->buf_c2s_len = 0;
    conn->buf_c2s_sent = 0;

    // 二.读取客户端新数据

    int n =
        read(conn->client_fd, conn->buf_c2s, sizeof(conn->buf_c2s));
    if (n < 0) { // 没读进去,要么阻塞要么出错
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 阻塞,下次再读
            return 0;
        } else {
            perror("read from client");
            return 1;
        }
    } else if (n == 0) {// 读到EOF,客户端主动关闭了
        conn->client_closed_r = 0;
        conn->state = eof_state;
    } else { // 读到了,记录下来
        conn->buf_c2s_len = n;
        conn->buf_c2s_sent = 0;
    }

    // 三.尝试发送刚读的数据
    // while (conn->buf_c2s_len > conn->buf_c2s_sent) {
    //     int n = write(conn->server_fd, conn->buf_c2s + conn->buf_c2s_sent, conn->buf_c2s_len - conn->buf_c2s_sent);

    //     if (n < 0) { // 没写进去,要么阻塞要么出错
    //         if (errno == EAGAIN || errno == EWOULDBLOCK) {
    //             // 阻塞,下次再写
    //             return 0;
    //         } else {
    //             perror("write to server");
    //             return 1;
    //         }
    //     }
    //     conn->buf_c2s_sent += n;
    // }

    // conn->buf_c2s_len = 0;
    // conn->buf_c2s_sent = 0;

    return handle_connection_c2s_forwarding(conn, eof_state, epfd);
}

int handle_connection_s2c_forwarding(conn_t* conn, conn_stat_t eof_state, int epfd) {
    conn->buf_s2c_len =
        Read(conn->server_fd, conn->buf_s2c, sizeof(conn->buf_s2c));

    // 如果读到了EOF,说明客户端主动关闭了
    if (conn->buf_s2c_len == 0) {
        conn->server_closed_r = 1;
        conn->state = eof_state;
        return 0;
    }

    // 如果返回 1,说明暂时没数据
    if (conn->buf_s2c_len < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
        else
            return 1;
    }

    // 读到的不是EOF,内容发送给客户端
    write(conn->client_fd, conn->buf_s2c, conn->buf_s2c_len);
    return 0;
}

int handle_connection_state(fd_event_t* fd_event, int epfd) {
    conn_t* conn = fd_event->conn;
    int is_client = fd_event->is_client;

    switch (conn->state) {
        case CONN_INIT:
            if (add_server_to_epoll(epfd, conn) != 0)
                conn->state = CONN_ERROR;
            return 0;
            break;

        case CONN_ACTIVE:
            if (is_client) { // 如果是客户端
                if (handle_connection_c2s_forwarding(
                    conn, CONN_HALF_CLOSED_BY_CLIENT, epfd) != 0)
                    conn->state = CONN_ERROR;
            } else { // 如果是服务器
                if (handle_connection_s2c_forwarding(
                    conn, CONN_HALF_CLOSED_BY_SERVER, epfd) != 0)
                    conn->state = CONN_ERROR;
            }
            return 0;
            break;

        case CONN_HALF_CLOSED_BY_CLIENT:
            // 客户端已经不再发送消息了,此时只有服务器能发送消息
            if (!is_client) {
                if (handle_connection_s2c_forwarding(conn, CONN_FULLY_CLOSED, epfd) != 0)
                    conn->state = CONN_ERROR;
            }
            return 0;
            break;

        case CONN_HALF_CLOSED_BY_SERVER:
            // 服务器已经不再发送消息了,此时只有客户端能发送消息
            if (is_client) {
                if (handle_connection_c2s_forwarding(conn, CONN_FULLY_CLOSED, epfd) != 0)
                    conn->state = CONN_ERROR;
            }
            return 0;
            break;

        case CONN_FULLY_CLOSED:
        case CONN_ERROR:
            if (conn->client_fd > 0) {
                Epoll_ctl(epfd, EPOLL_CTL_DEL, conn->client_fd, NULL);
                close(conn->client_fd);
            }
            if (conn->server_fd > 0) {
                Epoll_ctl(epfd, EPOLL_CTL_DEL, conn->server_fd, NULL);
                close(conn->server_fd);
            }
            free(conn);
            free(fd_event);
            return 0;
            break;

        default:
            break;
    }
    return 0;
}
