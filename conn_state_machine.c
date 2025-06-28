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

    printf("新连接client_fd为:%d\n", client_fd);

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
    } else {
        printf("%.*s", n, conn->parse_buffer + conn->parse_buffer_in); // 调试代码
        conn->parse_buffer_in += n;
    }

    //  异步,每读取一次都尝试解析请求行
    switch (conn->parse_state) {
        case PARSE_REQUEST_LINE:
            if (parse_request_line(conn) == 0) {
                // 对于 CONNECT 方法，不需要解析 Host 头，直接完成
                if (conn->req.method == CONNECT) {
                    conn->parse_state = PARSE_COMPLETE;
                } else {
                    conn->parse_state = PARSE_REQUEST_HOST;
                }
            }
            break;
        case PARSE_REQUEST_HOST:
            if (parse_request_host(conn) == 0)
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
    int parsed = sscanf(conn->parse_buffer + conn->parse_buffer_out, "%s %s %s", method, uri, version);

    printf("解析请求行: method='%s', uri='%s', version='%s'\n", method, uri, version); // 调试

    if (parsed < 3) {
        printf("请求行解析失败，只解析到 %d 个字段\n", parsed);
        return 1;
    }

    // 填充请求结构
    conn->req.method = parse_http_method(method);
    strcpy(conn->req.uri, uri);
    sscanf(version, "HTTP/%s", conn->req.proto_ver);
    strcpy(conn->req.proto, "HTTP");

    // 对于 CONNECT 方法，uri 就是 host:port 格式
    if (conn->req.method == CONNECT) {
        strcpy(conn->req.host, uri);  // CONNECT 方法中 uri 就是目标主机
        printf("CONNECT 请求，目标主机: %s\n", conn->req.host);
    }

    // 恢复"\r\n"并移动位置指针
    *end = '\r';
    conn->parse_buffer_out = (end - conn->parse_buffer) + 2;

    return 0;
}

int parse_request_host(conn_t* conn) {
    // 在缓冲区中寻找"\r\n"
    char* end = strstr(conn->parse_buffer + conn->parse_buffer_out, "\r\n");
    if (!end)
        return 1; // 请求行不完整

    *end = '\0'; // 暂时替换为结束符便于处理

    // 提取 Host 字段内容（包括端口）
    char* host_line = conn->parse_buffer + conn->parse_buffer_out;
    if (strncasecmp(host_line, "Host:", 5) == 0) {
        host_line += 5;
        while (*host_line == ' ' || *host_line == '\t') host_line++; // 跳过空格
        // 只拷贝到缓冲区末尾或遇到空白字符/回车
        size_t i = 0;
        while (i < sizeof(conn->req.host) - 1 && host_line[i] && host_line[i] != '\r' && host_line[i] != '\n' && host_line[i] != ' ' && host_line[i] != '\t') {
            conn->req.host[i] = host_line[i];
            i++;
        }
        conn->req.host[i] = '\0';
    }

    // 恢复"\r\n"并移动位置指针
    *end = '\r';
    conn->parse_buffer_out = (end - conn->parse_buffer) + 2;

    return 0;
}

int add_server_to_epoll(int epfd, conn_t* conn) {
    int server_fd;
    char host[1024];
    char port[16];

    printf("准备连接到服务器: %s\n", conn->req.host); // 调试信息

    // 解析 host:port 格式
    char* colon = strchr(conn->req.host, ':');
    if (colon) {
        // 有端口号
        *colon = '\0';
        strcpy(host, conn->req.host);
        strcpy(port, colon + 1);
        *colon = ':'; // 恢复
    } else {
        // 没有端口号，根据方法类型设置默认端口
        strcpy(host, conn->req.host);
        if (conn->req.method == CONNECT)
            strcpy(port, "443");
        else
            strcpy(port, "80");
    }

    printf("解析后 - 主机: %s, 端口: %s\n", host, port); // 调试信息

    server_fd = openConnectfd(host, port);

    if (server_fd <= 0) {
        printf("连接服务器失败: %s:%s, fd=%d\n", host, port, server_fd); // 调试信息
        perror("add_server_to_epoll->openConnectfd");

        // 向客户端发送错误响应
        const char* error_resp = "HTTP/1.1 502 Bad Gateway\r\nConnection: close\r\n\r\n";
        write(conn->client_fd, error_resp, strlen(error_resp));

        return 1;
    }

    printf("成功连接到服务器: %s:%s, server_fd=%d\n", host, port, server_fd); // 调试信息

    // 为 server_fd 注册 EPOLL_IN
    conn->server_fd = server_fd;

    fd_event_t* fd_event = Malloc(sizeof(fd_event_t));
    fd_event->conn = conn;
    fd_event->is_client = 0;

    conn->server_event = fd_event;

    struct epoll_event ev = { .events = EPOLLIN, .data.ptr = fd_event };
    Epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev);

    // 如果是CONNECT,准备异步发送响应
    if (conn->req.method == CONNECT) {
        const char* resp = "HTTP/1.1 200 Connection Established\r\n\r\n";
        conn->connect_resp_len = strlen(resp);
        strcpy(conn->connect_resp, resp);
        conn->connect_resp_sent = 0;

        // 设置状态为正在发送CONNECT响应
        conn->state = CONN_SENDING_CONNECT_RESP;

        // 注册客户端的EPOLLOUT事件来异步发送响应
        struct epoll_event ev = { .events = EPOLLIN | EPOLLOUT, .data.ptr = conn->client_event };
        Epoll_ctl(epfd, EPOLL_CTL_MOD, conn->client_fd, &ev);
    } else {
        conn->state = CONN_ACTIVE;
    }

    return 0;
}

int handle_connect_response_sending(conn_t* conn, int epfd) {
    // 尝试发送剩余的 CONNECT 响应
    while (conn->connect_resp_sent < conn->connect_resp_len) {
        int n = write(conn->client_fd,
            conn->connect_resp + conn->connect_resp_sent,
            conn->connect_resp_len - conn->connect_resp_sent);

        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 写缓冲区满了，等待下次 EPOLLOUT 事件
                return 0;
            } else {
                perror("发送 CONNECT 响应失败");
                return 1;
            }
        } else if (n == 0) {
            // 客户端关闭了连接
            return 1;
        }

        conn->connect_resp_sent += n;
    }

    // CONNECT 响应发送完成，切换到 ACTIVE 状态
    conn->state = CONN_ACTIVE;

    // 修改客户端事件，移除 EPOLLOUT，只保留 EPOLLIN
    struct epoll_event ev = { .events = EPOLLIN, .data.ptr = conn->client_event };
    Epoll_ctl(epfd, EPOLL_CTL_MOD, conn->client_fd, &ev);

    printf("CONNECT 响应发送完成，建立隧道连接\n"); // 调试代码
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
    }

    return 0;
}

int handle_connection_s2c_forwarding(conn_t* conn, conn_stat_t eof_state,
    int epfd) {

    // 一.首先尝试还没写完的数据
    write_result_t result = try_writing_to_peer(conn, epfd, 0);
    if (result == TRY_WRITE_ERR) // 出错
        return 1;
    else if (result == TRY_WRITE_BLOCKED) { // 阻塞,等 EPOLLOUT 再写
        return 0;
    } else if (result == TRY_WRITE_OK) {
        // 如果所有数据都被写完了,取消监听
        struct epoll_event ev = { .events = EPOLLIN, .data.ptr = conn->client_event };
        Epoll_ctl(epfd, EPOLL_CTL_MOD, conn->client_fd, &ev);
    }

    // 二.读取服务器新数据
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
        printf("%.*s", n, conn->buf_s2c); // 调试代码

        conn->buf_s2c_in = n;
        conn->buf_s2c_out = 0;
        // 继续写新读到的数据
        write_result_t result = try_writing_to_peer(conn, epfd, 0);
        if (result == TRY_WRITE_ERR) // 出错
            return 1;
        else if (result == TRY_WRITE_BLOCKED) { // 阻塞,等 EPOLLOUT 再写
            return 0;
        } else if (result == TRY_WRITE_OK) {
            // 如果所有数据都被写完了,取消监听
            struct epoll_event ev = { .events = EPOLLIN, .data.ptr = conn->client_event };
            Epoll_ctl(epfd, EPOLL_CTL_MOD, conn->client_fd, &ev);
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

        case CONN_SENDING_CONNECT_RESP:
            // 只处理客户端的 EPOLLOUT 事件
            if (is_client) {
                if (handle_connect_response_sending(conn, epfd) != 0)
                    conn->state = CONN_ERROR;
            }
            break;

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
                printf("关闭 client_fd %d 连接", conn->client_fd); // 调试代码
            }
            if (conn->server_fd > 0) {
                Epoll_ctl(epfd, EPOLL_CTL_DEL, conn->server_fd, NULL);
                close(conn->server_fd);
            }
            // 释放所有相关的内存
            if (conn->client_event) free(conn->client_event);
            if (conn->server_event) free(conn->server_event);
            free(conn);
            break;

        default:
            break;
    }

    return conn->state != CONN_ERROR ? 0 : 1;
}
