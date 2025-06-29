#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

#include "common/config.h"
#include "common/logger.h"
#include "common/rio.h"
#include "common/sys_wrap.h"
#include "http/http_util.h"
#include "http/http_respond.h"
#include "net/net_utils.h"
#include "net/ssl_util.h"

// 定义端口
#define HTTP_PORT "8080"
#define HTTPS_PORT "8443"

// 服务器连接类型
typedef enum {
    CONN_HTTP,          // 普通HTTP连接
    CONN_HTTPS_HANDSHAKE, // HTTPS握手中
    CONN_HTTPS_ACTIVE     // HTTPS活跃(已完成握手)
} server_conn_type_t;

// 服务器连接结构
typedef struct {
    int fd;                   // 客户端socket
    server_conn_type_t type;  // 连接类型
    SSL *ssl;                 // SSL连接(HTTPS使用)
    char buffer[MAXBUF];      // 输入缓冲区
    int buffer_size;          // 缓冲区已使用大小
    int processed;            // 已处理的数据量
} server_conn_t;

// 全局变量
static int g_http_fd = -1;           // HTTP监听socket
static int g_https_fd = -1;          // HTTPS监听socket
static int g_epfd = -1;              // epoll实例
static SSL_CTX *g_ssl_ctx = NULL;    // SSL上下文
static volatile int g_running = 1;   // 运行标志

// 信号处理函数
void signal_handler(int signum) {
    LOG_INFO("捕获信号 %d，准备关闭服务器", signum);
    g_running = 0;
}

// 初始化信号处理
void setup_signal_handlers() {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        LOG_ERROR("无法设置SIGINT处理器: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        LOG_ERROR("无法设置SIGTERM处理器: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

// 创建并初始化服务器连接结构
server_conn_t* create_server_conn(int fd, server_conn_type_t type) {
    server_conn_t* conn = (server_conn_t*)Malloc(sizeof(server_conn_t));
    if (!conn) {
        return NULL;
    }
    
    memset(conn, 0, sizeof(server_conn_t));
    conn->fd = fd;
    conn->type = type;
    
    // 如果是HTTPS，创建SSL对象
    if (type == CONN_HTTPS_HANDSHAKE) {
        conn->ssl = SSL_new(g_ssl_ctx);
        if (!conn->ssl) {
            LOG_ERROR("创建SSL对象失败");
            free(conn);
            return NULL;
        }
        SSL_set_fd(conn->ssl, fd);
    }
    
    return conn;
}

// 清理连接资源
void cleanup_connection(server_conn_t* conn) {
    if (!conn) return;
    
    if (conn->fd > 0) {
        Epoll_ctl(g_epfd, EPOLL_CTL_DEL, conn->fd, NULL);
        close(conn->fd);
    }
    
    if (conn->ssl) {
        SSL_shutdown(conn->ssl);
        SSL_free(conn->ssl);
    }
    
    free(conn);
}

// 处理HTTP连接上的数据
int handle_http_data(server_conn_t* conn) {
    // 读取请求数据
    int n = read(conn->fd, conn->buffer + conn->buffer_size, 
                 sizeof(conn->buffer) - conn->buffer_size);
                 
    if (n <= 0) {
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return 0; // 需要更多数据
        }
        return -1; // 连接关闭或错误
    }
    
    conn->buffer_size += n;
    
    // 检查是否收到完整请求（简单判断，实际应该更严格）
    char* end_headers = strstr(conn->buffer, "\r\n\r\n");
    if (!end_headers) {
        return 0; // 请求不完整，等待更多数据
    }
    
    // 处理HTTP响应
    http_request_t req;
    memset(&req, 0, sizeof(req));
    
    // 如果需要，可以手动解析请求而不是调用完整的parseHttpRequest
    // 因为我们已经读取了一部分数据
    
    // 处理请求，返回响应
    httpRespond(conn->fd);
    
    return 1; // 请求已处理
}

// 处理HTTPS握手
int handle_https_handshake(server_conn_t* conn) {
    int ret = SSL_accept(conn->ssl);
    if (ret == 1) {
        // 握手成功，转为活跃状态
        LOG_INFO("HTTPS握手成功，客户端: %d", conn->fd);
        conn->type = CONN_HTTPS_ACTIVE;
        return 0;
    }
    
    int err = SSL_get_error(conn->ssl, ret);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
        // 需要更多数据或写入缓冲区，继续等待事件
        return 0;
    }
    
    // 握手失败
    LOG_ERROR("SSL握手失败: %d", err);
    ERR_print_errors_fp(stderr);
    return -1;
}

// 处理HTTPS连接上的数据
int handle_https_data(server_conn_t* conn) {
    // 读取加密数据
    int n = SSL_read(conn->ssl, conn->buffer + conn->buffer_size,
                    sizeof(conn->buffer) - conn->buffer_size);
                    
    if (n <= 0) {
        int err = SSL_get_error(conn->ssl, n);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            return 0; // 需要更多数据
        }
        if (err == SSL_ERROR_ZERO_RETURN) {
            LOG_INFO("HTTPS连接正常关闭: %d", conn->fd);
        } else {
            LOG_ERROR("SSL读取错误: %d", err);
        }
        return -1; // 连接关闭或错误
    }
    
    conn->buffer_size += n;
    
    // 检查是否收到完整请求
    char* end_headers = strstr(conn->buffer, "\r\n\r\n");
    if (!end_headers) {
        return 0; // 请求不完整，等待更多数据
    }
    
    // 解析HTTP请求
    http_request_t req;
    memset(&req, 0, sizeof(req));
    
    // 在实际应用中，应该为HTTPS连接实现一个特殊的处理函数
    // 这里简化处理，仅返回一个基本响应
    const char* response = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: close\r\n"
        "\r\n"
        "Hello from HTTPS server!";
    
    SSL_write(conn->ssl, response, strlen(response));
    
    return 1; // 请求已处理
}

// 处理连接上的事件
int handle_connection_event(struct epoll_event* event) {
    server_conn_t* conn = (server_conn_t*)event->data.ptr;
    
    if (!conn) return -1;
    
    // 根据连接类型处理数据
    switch (conn->type) {
        case CONN_HTTP:
            if (handle_http_data(conn) != 0) {
                // 处理完成或出错，关闭连接
                LOG_INFO("HTTP请求处理完成，关闭连接: %d", conn->fd);
                cleanup_connection(conn);
                return 1;
            }
            break;
            
        case CONN_HTTPS_HANDSHAKE:
            if (handle_https_handshake(conn) != 0) {
                // 握手失败，关闭连接
                LOG_ERROR("HTTPS握手失败，关闭连接: %d", conn->fd);
                cleanup_connection(conn);
                return 1;
            }
            break;
            
        case CONN_HTTPS_ACTIVE:
            if (handle_https_data(conn) != 0) {
                // 处理完成或出错，关闭连接
                LOG_INFO("HTTPS请求处理完成，关闭连接: %d", conn->fd);
                cleanup_connection(conn);
                return 1;
            }
            break;
            
        default:
            LOG_ERROR("未知连接类型: %d", conn->type);
            cleanup_connection(conn);
            return 1;
    }
    
    return 0;
}

// 接受新连接
void accept_new_connection(int listen_fd, server_conn_type_t type) {
    struct sockaddr_storage client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        LOG_ERROR("接受连接失败: %s", strerror(errno));
        return;
    }
    
    // 设置非阻塞
    make_socket_non_blocking(client_fd);
    
    // 创建连接对象
    server_conn_t* conn = create_server_conn(client_fd, type);
    if (!conn) {
        LOG_ERROR("创建连接对象失败");
        close(client_fd);
        return;
    }
    
    // 注册到epoll
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET; // 边缘触发模式
    ev.data.ptr = conn;
    
    if (Epoll_ctl(g_epfd, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
        LOG_ERROR("添加到epoll失败: %s", strerror(errno));
        cleanup_connection(conn);
        return;
    }
    
    LOG_INFO("接受新%s连接: %d", (type == CONN_HTTP) ? "HTTP" : "HTTPS", client_fd);
}

// 初始化SSL上下文
int init_ssl_context() {
    // 初始化OpenSSL
    init_ssl();
    
    // 创建SSL上下文
    const SSL_METHOD* method = TLS_server_method();
    g_ssl_ctx = SSL_CTX_new(method);
    if (!g_ssl_ctx) {
        LOG_ERROR("创建SSL上下文失败");
        return -1;
    }
    
    // 加载证书和私钥
    if (SSL_CTX_use_certificate_file(g_ssl_ctx, "server.crt", SSL_FILETYPE_PEM) <= 0) {
        LOG_ERROR("加载SSL证书失败");
        ERR_print_errors_fp(stderr);
        return -1;
    }
    
    if (SSL_CTX_use_PrivateKey_file(g_ssl_ctx, "server.key", SSL_FILETYPE_PEM) <= 0) {
        LOG_ERROR("加载SSL私钥失败");
        ERR_print_errors_fp(stderr);
        return -1;
    }
    
    // 验证私钥
    if (!SSL_CTX_check_private_key(g_ssl_ctx)) {
        LOG_ERROR("SSL私钥验证失败");
        ERR_print_errors_fp(stderr);
        return -1;
    }
    
    LOG_INFO("SSL上下文初始化成功");
    return 0;
}

// 初始化HTTP和HTTPS监听
int init_listeners() {
    // 创建HTTP监听
    g_http_fd = openListenfd(HTTP_PORT);
    if (g_http_fd < 0) {
        LOG_ERROR("创建HTTP监听失败");
        return -1;
    }
    
    // 创建HTTPS监听
    g_https_fd = openListenfd(HTTPS_PORT);
    if (g_https_fd < 0) {
        LOG_ERROR("创建HTTPS监听失败");
        close(g_http_fd);
        return -1;
    }
    
    // 设置非阻塞
    make_socket_non_blocking(g_http_fd);
    make_socket_non_blocking(g_https_fd);
    
    // 开始监听
    Listen(g_http_fd, EPOLL_LEN);
    Listen(g_https_fd, EPOLL_LEN);
    
    LOG_INFO("HTTP服务器监听端口 %s", HTTP_PORT);
    LOG_INFO("HTTPS服务器监听端口 %s", HTTPS_PORT);
    
    return 0;
}

int main() {
    // 初始化日志
    log_set_level(LOG_INFO);
    LOG_INFO("启动Web服务器...");
    
    // 设置信号处理
    setup_signal_handlers();
    
    // 初始化SSL
    if (init_ssl_context() != 0) {
        LOG_FATAL("SSL初始化失败");
        return 1;
    }
    
    // 创建HTTP和HTTPS监听
    if (init_listeners() != 0) {
        LOG_FATAL("创建监听失败");
        return 1;
    }
    
    // 创建epoll实例
    g_epfd = epoll_create1(0);
    if (g_epfd < 0) {
        LOG_FATAL("创建epoll实例失败: %s", strerror(errno));
        return 1;
    }
    
    // 注册监听socket到epoll
    struct epoll_event ev_http, ev_https;
    
    ev_http.events = EPOLLIN;
    ev_http.data.ptr = NULL; // 用NULL表示HTTP监听socket
    Epoll_ctl(g_epfd, EPOLL_CTL_ADD, g_http_fd, &ev_http);
    
    ev_https.events = EPOLLIN;
    ev_https.data.ptr = (void*)-1; // 用-1表示HTTPS监听socket
    Epoll_ctl(g_epfd, EPOLL_CTL_ADD, g_https_fd, &ev_https);
    
    // 事件循环
    struct epoll_event events[EPOLL_LEN];
    
    while (g_running) {
        int nfds = epoll_wait(g_epfd, events, EPOLL_LEN, 1000);
        
        if (nfds < 0) {
            if (errno == EINTR) continue;
            LOG_ERROR("epoll_wait错误: %s", strerror(errno));
            break;
        }
        
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.ptr == NULL) {
                // HTTP监听socket
                accept_new_connection(g_http_fd, CONN_HTTP);
            } else if (events[i].data.ptr == (void*)-1) {
                // HTTPS监听socket
                accept_new_connection(g_https_fd, CONN_HTTPS_HANDSHAKE);
            } else {
                // 客户端连接
                handle_connection_event(&events[i]);
            }
        }
    }
    
    // 清理资源
    LOG_INFO("服务器正在关闭...");
    
    if (g_http_fd >= 0) close(g_http_fd);
    if (g_https_fd >= 0) close(g_https_fd);
    if (g_epfd >= 0) close(g_epfd);
    
    if (g_ssl_ctx) {
        SSL_CTX_free(g_ssl_ctx);
    }
    
    cleanup_ssl();
    
    LOG_INFO("服务器已关闭");
    return 0;
}
