#ifndef CONN_STATE_MACHINE_H
#define CONN_STATE_MACHINE_H

#include "config.h"
#include"http_util.h"

typedef enum {
  CONN_INIT,                  // 初始状态,连接到服务器
  CONN_ACTIVE,                // 活跃中
  CONN_HALF_CLOSED_BY_CLIENT, // 客户端半关闭(半关闭读)
  CONN_HALF_CLOSED_BY_SERVER, // 服务器半关闭(半关闭读)
  CONN_FULLY_CLOSED,          // 全部关闭
  CONN_ERROR,                 // 连接错误
} conn_stat_t;

typedef enum {
  PARSE_REQUEST_LINE,
  PARSE_COMPLETE,
} http_parse_state_t;

typedef enum {
  TRY_WRITE_OK = 0,
  TRY_WRITE_BLOCKED = 1,
  TRY_WRITE_ERR = -1
} write_result_t;


struct fd_event_t;
typedef struct fd_event_t fd_event_t;

typedef struct conn_t {
  int client_fd;     // 客户端文件描述符
  int server_fd;     // 服务器文件描述符
  conn_stat_t state; // 当前状态

  char buf_c2s[MAXBUF];
  char buf_s2c[MAXBUF];
  int buf_c2s_in;  // 客户端缓冲区接收到的数据长度
  int buf_s2c_in;  // 服务器缓冲区接收到的数据长度
  int buf_c2s_out; // 户端缓缓冲区已发送的数据长度
  int buf_s2c_out; // 服务器缓冲区已发送的数据长度

  int client_closed_r; // 客户端是否关闭读
  int client_closed_w; // 客户端是否关闭写
  int server_closed_r; // 服务器是否关闭读
  int server_closed_w; // 服务器是否关闭写

  fd_event_t* client_event;
  fd_event_t* server_event;

  // HTTP 解析相关
  http_parse_state_t parse_state;
  char parse_buffer[MAXBUF];
  int parse_buffer_in;
  int parse_buffer_out;
  http_request_t req;
} conn_t;

typedef struct fd_event_t {
  conn_t* conn;
  int is_client;
} fd_event_t;

int add_client_to_epoll(int epfd, int listen_fd);
int try_parse_http_request(conn_t* conn);
int parse_request_line(conn_t* conn);
int add_server_to_epoll(int epfd, conn_t* conn);

int handle_connection_state(fd_event_t* fd_event, int epfd);
#endif // CONN_STATE_MACHINE_H
