#ifndef HTTP_UTIL_H
#define HTTP_UTIL_H

#include <stdio.h>

// 实现了GET和HEAD方法,后续若有API需求可以拓展POST等方法
typedef enum { CONNECT, GET, HEAD, UNSUPPORTED } http_method_t;

// 请求头结构体
typedef struct {
    // 请求行
    http_method_t method; // 方法枚举
    char uri[256];        // 请求路径
    char proto[16];       // 协议名，如 "HTTP"
    char proto_ver[16];   // 协议版本，如 "1.1"

    // 字段
    char host[1024];      // Host 头
    char user_agent[512]; // User-Agent 头,记录客户端信息
    char connection
        [32]; // 决定处理完本次连接后是否断开TCP连接,若为keep-alive后续可以复用同一个TCP提高效率
} http_request_t;

int parseHttpRequest(int client_fd, http_request_t* req);

// MIME 文件类型结构体
typedef const char* mime_t;

typedef struct {
    const char* ext;  // 文件拓展名
    mime_t mime_type; // MIME 类型
} mime_map_t;

extern const mime_map_t mime_map[];

mime_t getMimeType(const char* filepath);

void httpError(int client_fd, const char* message);

#endif // HTTP_UTIL_H
