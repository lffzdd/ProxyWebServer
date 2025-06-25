#ifndef CONN_STATE_MACHINE_H
#define CONN_STATE_MACHINE_H

#include"config.h"

typedef enum {
    CONN_INIT,// 连接初始化
    CONN_READING,// 读取
    CONN_FOWARDING,// 转发
    CONN_HALF_CLOSED_BY_CLIENT,// 客户端半关闭
    CONN_HALF_CLOSED_BY_SERVER,// 服务器半关闭
    CONN_CLOSED,// 连接关闭
    CONN_ERROR,// 连接错误
}conn_stat_t;

typedef struct conn_t {
    int client_fd; // 客户端文件描述符
    int server_fd; // 服务器文件描述符
    conn_stat_t state; // 当前状态

    char buf_c2s[MAXBUF];
    char buf_s2c[MAXBUF];
    int buf_c2s_len; // 客户端缓冲区长度
    int buf_s2c_len; // 服务器缓冲区长度

    int client_closed; // 客户端是否关闭
    int server_closed; // 服务器是否关闭


    // void (*on_read)(struct conn_state_machine* self); // 读取回调
    // void (*on_forward)(struct conn_state_machine* self); // 转发回调
    // void (*on_half_close_by_client)(struct conn_state_machine* self); // 客户端半关闭回调
    // void (*on_half_close_by_server)(struct conn_state_machine* self); // 服务器半关闭回调
    // void (*on_close)(struct conn_state_machine* self); // 连接关闭回调
    // void (*on_error)(struct conn_state_machine* self); // 错误回调
} conn_t;

int handle_connection_state(conn_t* conn, int ready_fd, int epfd);
#endif // CONN_STATE_MACHINE_H