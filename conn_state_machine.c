#include "conn_state_machine.h"
#include "http_util.h"
#include "net_utils.h"
#include <errno.h>
#include <malloc.h>
#include <sys/epoll.h>
#include <unistd.h>
#include<string.h>

int add_connection_to_epoll(int epfd, int listen_fd) {
    int client_fd = acceptClientfd(listen_fd);
    if (client_fd < 0) {
        perror("acceptClientfd");
        return -1;
    }


    http_request_t req = { 0 };
    if (parseHttpRequest(client_fd, &req) != 0) {
        close(client_fd);
        return -1;
    }

    int server_fd;
    if (req.method == CONNECT)
        server_fd = openConnectfd(req.host, "443");
    else
        server_fd = openConnectfd(req.host, "80");

    if (server_fd < 0) {
        perror("openConnectfd");
        close(client_fd);
        return -1;
    }

    conn_t* conn = malloc(sizeof(conn_t));
    if (!conn) {
        perror("malloc");
        close(client_fd);
        close(server_fd);
        return -1;
    }

    memset(conn, 0, sizeof(conn_t));
    conn->client_fd = client_fd;
    conn->server_fd = server_fd;
    conn->state = CONN_ACTIVE;

    struct epoll_event client_ev = { .events = EPOLLIN, .data.ptr = conn };
    struct epoll_event server_ev = { .events = EPOLLIN, .data.ptr = conn };

    epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &client_ev);
    epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &server_ev);

    return 0;
}

int handle_connection_c2s_forwarding(conn_t* conn, conn_stat_t eof_state) {
    conn->buf_c2s_len =
        read(conn->client_fd, conn->buf_c2s, sizeof(conn->buf_c2s));

    // 如果读到了EOF,说明客户端主动关闭了
    if (conn->buf_c2s_len == 0) {
        conn->client_closed_r = 1;
        conn->state = eof_state;
        return 0;
    }

    // 如果返回-1,说明暂时没数据
    if (conn->buf_c2s_len < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
        else {
            conn->state = CONN_ERROR;
            return -1;
        }
    }

    // 读到的不是EOF,内容发送给服务器
    write(conn->server_fd, conn->buf_c2s, conn->buf_c2s_len);
    return 0;
}

int handle_connection_s2c_forwarding(conn_t* conn, conn_stat_t eof_state) {
    conn->buf_s2c_len =
        read(conn->server_fd, conn->buf_s2c, sizeof(conn->buf_s2c));

    // 如果读到了EOF,说明客户端主动关闭了
    if (conn->buf_s2c_len == 0) {
        conn->server_closed_r = 1;
        conn->state = eof_state;
        return 0;
    }

    // 如果返回-1,说明暂时没数据
    if (conn->buf_s2c_len < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
        else {
            conn->state = CONN_ERROR;
            return -1;
        }
    }

    // 读到的不是EOF,内容发送给客户端
    write(conn->client_fd, conn->buf_s2c, conn->buf_s2c_len);
    return 0;
}

int handle_connection_state(conn_t* conn, int ready_fd, int epfd) {
    switch (conn->state) {
        case CONN_ACTIVE:
            if (conn->client_fd == ready_fd) { // 如果是客户端
                if (handle_connection_c2s_forwarding(
                    conn, CONN_HALF_CLOSED_BY_CLIENT) != 0)
                    conn->state = CONN_ERROR;
            } else if (conn->server_fd == ready_fd) { // 如果是服务器
                if (handle_connection_s2c_forwarding(
                    conn, CONN_HALF_CLOSED_BY_SERVER) != 0)
                    conn->state = CONN_ERROR;
            }
            break;

        case CONN_HALF_CLOSED_BY_CLIENT:
            // 客户端已经不再发送消息了,此时只有服务器能发送消息
            if (conn->server_fd == ready_fd) {
                if (handle_connection_s2c_forwarding(conn, CONN_FULLY_CLOSED) != 0)
                    conn->state = CONN_ERROR;
            }
            break;

        case CONN_HALF_CLOSED_BY_SERVER:
            // 服务器已经不再发送消息了,此时只有客户端能发送消息
            if (conn->client_fd == ready_fd) {
                if (handle_connection_c2s_forwarding(conn, CONN_FULLY_CLOSED) != 0)
                    conn->state = CONN_ERROR;
            }
            break;

        case CONN_FULLY_CLOSED:
        case CONN_ERROR:
            epoll_ctl(epfd, EPOLL_CTL_DEL, conn->client_fd, NULL);
            epoll_ctl(epfd, EPOLL_CTL_DEL, conn->server_fd, NULL);
            close(conn->client_fd);
            close(conn->server_fd);
            free(conn);
            return 0;
            break;

        default:
            break;
    }
    return 0;
}
