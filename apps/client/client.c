#define _POSIX_C_SOURCE 200122L
#include <fcntl.h>
#include <netdb.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "common/config.h"
#include "common/sys_wrap.h"
#include "net/net_utils.h"

// SSL非阻塞连接函数，使用epoll处理
int connect_ssl_nonblock(SSL *ssl, int epfd) {
    int server_fd = SSL_get_fd(ssl);
    struct epoll_event ev = {.events = EPOLLIN | EPOLLOUT};

    while (1) {
        int ret = SSL_connect(ssl);

        if (ret == 1) // SSL连接成功
            return 0;

        int err = SSL_get_error(ssl, ret);

        if (err == SSL_ERROR_WANT_READ) {
            // 等待可读事件
            ev.events = EPOLLIN;
            Epoll_ctl(epfd, EPOLL_CTL_MOD, server_fd, &ev);

            struct epoll_event events[1];
            int nready = epoll_wait(epfd, events, 1, -1);
            if (nready < 0) {
                perror("epoll_wait in SSL_connect");
                return -1;
            }
        } else if (err == SSL_ERROR_WANT_WRITE) {
            // 等待可写事件
            ev.events = EPOLLOUT;
            Epoll_ctl(epfd, EPOLL_CTL_MOD, server_fd, &ev);

            struct epoll_event events[1];
            int nready = epoll_wait(epfd, events, 1, -1);
            if (nready < 0) {
                perror("epoll_wait in SSL_connect");
                return -1;
            }
        } else {
            // 真正的错误
            fprintf(stderr, "SSL_connect error: %d\n", err);
            ERR_print_errors_fp(stderr);
            return -1;
        }
    }

    return 0;
}

int main(int argc, char const *argv[]) {
    // 建立 socket 通道
    int server_fd = openConnectfd("www.bilibili.com", "443");
    if (server_fd < 0) {
        perror("无法连接到服务器");
        return 1;
    }

    // 创建epoll实例
    int epfd = epoll_create1(0);
    if (epfd < 0) {
        perror("epoll_create1");
        close(server_fd);
        return 1;
    }

    // 注册服务器socket到epoll
    struct epoll_event ev = {.events = EPOLLIN | EPOLLOUT};
    Epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev);

    // 初始化 SSL
    SSL_load_error_strings();
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());

    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, server_fd);

    // 使用epoll版本的SSL连接
    if (connect_ssl_nonblock(ssl, epfd) != 0) {
        fprintf(stderr, "SSL连接失败\n");
        goto cleanup;
    }

    printf("SSL连接成功\n");

    char buf[MAXBUF];
    int n;

    printf("发送请求\n\n");
    int req_fd = Open("req.txt", O_RDONLY);
    while ((n = Read(req_fd, buf, sizeof(buf))) > 0) {
        int ret = SSL_write(ssl, buf, n);
        printf("%s", buf);
        if (ret <= 0) {
            fprintf(stderr, "SSL_write失败: %d\n", SSL_get_error(ssl, ret));
            goto cleanup;
        }
    }
    close(req_fd);

    printf("收到响应\n\n");
    int save_fd = open("save.html", O_WRONLY | O_CREAT | O_TRUNC, 0644);

    // 修改epoll事件为只监听读事件
    ev.events = EPOLLIN;
    Epoll_ctl(epfd, EPOLL_CTL_MOD, server_fd, &ev);

    struct epoll_event events[1];
    int timeout_ms = 5000; // 5秒超时

    while (1) {
        int nready = epoll_wait(epfd, events, 1, timeout_ms);

        if (nready < 0) {
            perror("epoll_wait");
            break;
        }

        if (nready == 0) {
            printf("等待响应超时\n");
            break;
        }

        // 有可读事件，尝试读取数据
        n = SSL_read(ssl, buf, sizeof(buf));

        if (n > 0) {
            // 成功读取数据
            write(save_fd, buf, n);
            continue;
        }

        // 处理错误
        int err = SSL_get_error(ssl, n);
        if (err == SSL_ERROR_WANT_READ) {
            // 继续等待可读事件
            continue;
        } else if (err == SSL_ERROR_WANT_WRITE) {
            // 需要等待可写事件
            ev.events = EPOLLOUT;
            Epoll_ctl(epfd, EPOLL_CTL_MOD, server_fd, &ev);
            continue;
        } else if (err == SSL_ERROR_ZERO_RETURN) {
            // 连接正常关闭
            printf("连接已关闭\n");
            break;
        } else {
            // 其他错误
            fprintf(stderr, "SSL_read错误: %d\n", err);
            ERR_print_errors_fp(stderr);
            break;
        }
    }

cleanup:
    close(save_fd);
    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close(server_fd);
    close(epfd);

    return 0;
}
