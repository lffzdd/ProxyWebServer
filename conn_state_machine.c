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

/* 获取请求方法 */
int try_parse_http_request(conn_t* conn) {
    // 读取请求数据
    int n = read(conn->client_fd, conn->parse_buffer + conn->parse_buffer_in,
        sizeof(conn->parse_buffer) - conn->parse_buffer_in);

    if (n < 0) {                                   // 阻塞或者出错
        if (errno == EAGAIN || errno == EWOULDBLOCK) // 阻塞, return 0下次读
            return 0;
        else {
            return 1;
        };
    } else if (n == 0 &&
        conn->parse_buffer_in == 0) { // 读到EOF,且之前没读到过数据
        return 1;
    } else
        conn->parse_buffer_in += n;

    //  异步,每读取一次都尝试解析请求行
    switch (conn->parse_state) {
        case PARSE_REQUEST_LINE:
            if (parse_request_line(conn) == 0)
                conn->parse_state = PARSE_COMPLETE;
            break;

        default:
            break;
    }

    return conn->parse_state == PARSE_COMPLETE ? 0 : 1;
}

// 解析请求行
int parse_request_line(conn_t* conn) {
    // 在缓冲区中寻找"\r\n"
    char* end = strstr(conn->parse_buffer + conn->parse_buffer_out, "\r\n");
    if (!end)
        return 1; // 请求行不完整

    *end = '\0'; // 暂时替换为结束符便于处理

    // 解析请求行
    char method[64], uri[256], version[64];
    sscanf(conn->parse_buffer + conn->parse_buffer_out, "%s %s %s", method, uri,
        version);

    // 填充请求结构
    conn->req.method = parse_http_method(method);
    strcpy(conn->req.uri, uri); // uri 可能是htts://xxx:443,也可能没有https://
    sscanf(version, "HTTP/%s", conn->req.proto_ver);
    strcpy(conn->req.proto, "HTTP");

    // 恢复"\r\n"并移动位置指针
    *end = '\r';
    conn->parse_buffer_out = (end - conn->parse_buffer) + 2;

    return 0;
}

int add_server_to_epoll(int epfd, conn_t* conn) {
    int server_fd;

    if (conn->req.method == CONNECT)
        server_fd = openConnectfd(conn->req.host, "443");
    else
        server_fd = openConnectfd(conn->req.host, "80");

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

    conn->state = CONN_ACTIVE;

    return 0;
}

/* 转发消息 */

write_result_t try_writing_to_peer(conn_t* conn, int epfd, int is_c2s) {
    char* buf;
    int* buf_in, * buf_out;
    int peer_fd;
    fd_event_t* fd_event_peer;

    if (is_c2s) {
        buf = conn->buf_c2s;
        buf_in = &(conn->buf_c2s_in);
        buf_out = &(conn->buf_c2s_out);

        peer_fd = conn->server_fd;
        fd_event_peer = conn->server_event;
    } else {
        buf = conn->buf_s2c;
        buf_in = &(conn->buf_s2c_in);
        buf_out = &(conn->buf_s2c_out);

        peer_fd = conn->client_fd;
        fd_event_peer = conn->client_event;
    }

    while (*buf_in > *buf_out) {
        int n = write(peer_fd, buf + *buf_out,
            *buf_in - *buf_out);

        if (n < 0) { // 没写进去,要么阻塞要么出错
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 阻塞,注册 EPOLLOUT事件,让它写完
                struct epoll_event ev = { .events = EPOLLIN | EPOLLOUT,
                                         .data.ptr = fd_event_peer };
                Epoll_ctl(epfd, EPOLL_CTL_MOD, peer_fd, &ev);
                return TRY_WRITE_BLOCKED;
            } else {
                fprintf(stderr, "[%s -> %s] , write to %s\n", __FILE__, __func__, is_c2s ? "server" : "client");
                return TRY_WRITE_ERR;
            }
        }
        // 调试代码
        printf("%.*s", n, buf + *buf_out);

        *buf_out += n;
    }


    *buf_in = 0;
    *buf_out = 0;

    return TRY_WRITE_OK;
}

int handle_connection_c2s_forwarding(conn_t* conn, conn_stat_t eof_state,
    int epfd) {

    // 一.首先尝试还没写完的数据
    write_result_t result = try_writing_to_peer(conn, epfd, 1);
    if (result == TRY_WRITE_ERR) // 出错
        return 1;
    else if (result == TRY_WRITE_BLOCKED) { // 阻塞,等 EPOLLOUT 再写
        return 0;
    } else if (result == TRY_WRITE_OK) {
        // 如果所有数据都被写完了,取消监听
        struct epoll_event ev = { .events = EPOLLIN, .data.ptr = conn->server_event };
        Epoll_ctl(epfd, EPOLL_CTL_MOD, conn->server_fd, &ev);
    }


    // 二.读取客户端新数据
    int n = read(conn->client_fd, conn->buf_c2s, sizeof(conn->buf_c2s));
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 阻塞,下次再读
            return 0;
        } else {
            perror("read from client");
            return 1;
        }
    } else if (n == 0) { // 读到EOF,客户端主动关闭了
        conn->client_closed_r = 1;
        conn->state = eof_state;
        return 0;
    } else { // 读到了,更新缓冲区
        conn->buf_c2s_in = n;
        conn->buf_c2s_out = 0;
        // 继续写新读到的数据
        if (try_writing_to_peer(conn, epfd, 1) == 1)
            return 1;
        else if (errno == EAGAIN || errno == EWOULDBLOCK) { // 阻塞
            return 0;
        }
    }

    return 0;
}

int handle_connection_s2c_forwarding(conn_t* conn, conn_stat_t eof_state,
    int epfd) {

    // 一.首先尝试还没写完的数据
    if (try_writing_to_peer(conn, epfd, 0) == 1) // 出错
        return 1;
    else if (errno == EAGAIN || errno == EWOULDBLOCK) { // 阻塞
        return 0;
    }

    // 如果所有数据都被写完了,取消监听
    struct epoll_event ev = { .events = EPOLLIN, .data.ptr = conn->client_event };
    Epoll_ctl(epfd, EPOLL_CTL_MOD, conn->client_fd, &ev);

    // 二.读取客户端新数据
    int n = read(conn->server_fd, conn->buf_s2c, sizeof(conn->buf_s2c));
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 阻塞,下次再读
            return 0;
        } else {
            perror("read from client");
            return 1;
        }
    } else if (n == 0) { // 读到EOF,客户端主动关闭了
        conn->server_closed_r = 1;
        conn->state = eof_state;
        return 0;
    } else { // 读到了,更新缓冲区
        conn->buf_s2c_in = n;
        conn->buf_s2c_out = 0;
        // 继续写新读到的数据
        if (try_writing_to_peer(conn, epfd, 0) == 1)
            return 1;
        else if (errno == EAGAIN || errno == EWOULDBLOCK) { // 阻塞
            return 0;
        }
    }

    return 0;
}

int handle_connection_state(fd_event_t* fd_event, int epfd) {
    conn_t* conn = fd_event->conn;
    int is_client = fd_event->is_client;

    switch (conn->state) {
        case CONN_INIT: {
                int ret = try_parse_http_request(conn);
                if (ret == 1) {
                    conn->state = CONN_ERROR;
                    break;
                } else if (ret == 0 && conn->parse_state !=
                    PARSE_COMPLETE) // 没读完,直接返回等待下次读取
                    break;

                if (add_server_to_epoll(epfd, conn) != 0)
                    conn->state = CONN_ERROR;
            }   break;

        case CONN_ACTIVE:
            if (is_client) { // 如果是客户端
                if (handle_connection_c2s_forwarding(conn, CONN_HALF_CLOSED_BY_CLIENT,
                    epfd) != 0)
                    conn->state = CONN_ERROR;
            } else { // 如果是服务器
                if (handle_connection_s2c_forwarding(conn, CONN_HALF_CLOSED_BY_SERVER,
                    epfd) != 0)
                    conn->state = CONN_ERROR;
            }
            break;

        case CONN_HALF_CLOSED_BY_CLIENT:
            // 客户端已经不再发送消息了,此时只有服务器能发送消息
            if (!is_client) {
                if (handle_connection_s2c_forwarding(conn, CONN_FULLY_CLOSED, epfd) != 0)
                    conn->state = CONN_ERROR;
            }
            break;

        case CONN_HALF_CLOSED_BY_SERVER:
            // 服务器已经不再发送消息了,此时只有客户端能发送消息
            if (is_client) {
                if (handle_connection_c2s_forwarding(conn, CONN_FULLY_CLOSED, epfd) != 0)
                    conn->state = CONN_ERROR;
            }
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
            break;

        default:
            break;
    }

    return conn->state != CONN_ERROR ? 0 : 1;
}
