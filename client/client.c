#define _POSIX_C_SOURCE 200122L
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include<fcntl.h>
#include<openssl/ssl.h>
#include<openssl/err.h>
#include<sys/select.h>

#include "../config.h"
#include "../net_utils.h"
#include"../sys_wrap.h"


int connect_ssl_nonblock(SSL* ssl) {
    int ret, err;
    while (1) {
        ret = SSL_connect(ssl);
        if (ret == 1) // 成功
            return 0;

        err = SSL_get_error(ssl, ret);
        if (err == SSL_ERROR_WANT_READ) {
            //等待 socket可读
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(SSL_get_fd(ssl), &rfds);
            select(SSL_get_fd(ssl) + 1, &rfds, NULL, NULL, NULL);
        } else if (err == SSL_ERROR_WANT_WRITE) {
            // 等待 socket 可写
            fd_set wfds;
            FD_ZERO(&wfds);
            FD_SET(SSL_get_fd(ssl), &wfds);
            select(SSL_get_fd(ssl) + 1, NULL, &wfds, NULL, NULL);
        } else {
            // 真正的错误
            fprintf(stderr, "SSL_connect error: %d\n", err);
            ERR_print_errors_fp(stderr);
            return 1;
        }
    }

    return 0;
}

int main(int argc, char const* argv[]) {
    // 建立 socket 通道
    int server_fd = openConnectfd("www.bilibili.com", "443");
    if (server_fd < 0) {
        perror("无法连接到服务器");
        return 1;
    }

    // 初始化 SSL 握手配置
    SSL_load_error_strings();
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());

    SSL* ssl = SSL_new(ctx);// 创建一个ssl会话对象
    SSL_set_fd(ssl, server_fd);// 将该ssl同socket绑定
    connect_ssl_nonblock(ssl); // socket为非阻塞

    char buf[MAXBUF];
    int n;

    printf("发送请求\n\n");
    int req_fd = Open("req.txt", O_RDONLY);
    while ((n = Read(req_fd, buf, sizeof(buf))) > 0) {
        int ret = SSL_write(ssl, buf, n);
        printf("%s", buf);
        if (ret <= 0)
            fprintf(stderr, "SSL_write失败: %d\n", SSL_get_error(ssl, ret));
    }
    close(req_fd);

    printf("收到响应\n\n");
    int save_fd = open("save.html", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    while (1) {
        n = SSL_read(ssl, buf, sizeof(buf));

        if (n > 0) {
            // 成功读取数据
            write(save_fd, buf, n);
            continue;
        }

        // 处理错误
        int err = SSL_get_error(ssl, n);
        if (err == SSL_ERROR_WANT_READ) {
            // 等待数据可读
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(server_fd, &rfds);

            // 设置5秒超时
            struct timeval tv = { 5, 0 };
            int select_ret = select(server_fd + 1, &rfds, NULL, NULL, &tv);

            if (select_ret <= 0) {
                if (select_ret == 0)
                    printf("等待响应超时\n");
                else
                    perror("select");
                break;
            }
            continue; // 再次尝试读取
        } else if (err == SSL_ERROR_WANT_WRITE) {
            // 等待可写
            fd_set wfds;
            FD_ZERO(&wfds);
            FD_SET(server_fd, &wfds);
            select(server_fd + 1, NULL, &wfds, NULL, NULL);
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
    close(save_fd);

    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);

    close(server_fd);

    return 0;
}
