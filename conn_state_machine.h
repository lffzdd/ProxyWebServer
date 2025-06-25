#ifndef CONN_STATE_MACHINE_H
#define CONN_STATE_MACHINE_H

#include "config.h"

typedef enum {
    CONN_ACTIVE,                // 活跃中
    CONN_HALF_CLOSED_BY_CLIENT, // 客户端半关闭(半关闭读)
    CONN_HALF_CLOSED_BY_SERVER, // 服务器半关闭(半关闭读)
    CONN_FULLY_CLOSED,          // 全部关闭
    CONN_ERROR,                 // 连接错误
} conn_stat_t;

typedef struct conn_t {
    int client_fd;     // 客户端文件描述符
    int server_fd;     // 服务器文件描述符
    conn_stat_t state; // 当前状态

    char buf_c2s[MAXBUF];
    char buf_s2c[MAXBUF];
    int buf_c2s_len; // 客户端缓冲区长度
    int buf_s2c_len; // 服务器缓冲区长度

    int client_closed_r; // 客户端是否关闭读
    int client_closed_w; // 客户端是否关闭写
    int server_closed_r; // 服务器是否关闭读
    int server_closed_w; // 服务器是否关闭写

} conn_t;

int add_connection_to_epoll(int epfd, int listen_fd);

int handle_connection_state(conn_t* conn, int ready_fd, int epfd);
#endif // CONN_STATE_MACHINE_H
