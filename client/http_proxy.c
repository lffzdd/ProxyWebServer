#include "http_proxy.h"
#include "config.h"
#include "http_util.h"
#include "net_utils.h"
#include "rio.h"
#include <fcntl.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

static SSL* init_ssl(int server_fd) {
    SSL_load_error_strings(); // 加载用于错误信息的字符串描述,不然ERR_print...只会打印数字

    const SSL_METHOD* method =
        TLS_client_method(); // 返回一个适用于客户端连接的通用 TLS
    // 方法结构体指针,SSL_METHOD
    // 是一种“策略模板”，决定了连接的行为（协议版本、握手流程等）,TLS_client_method()
    // 会自动协商支持的最高版本的 TLS（如 TLS 1.3）
    SSL_CTX* ctx = SSL_CTX_new(
        method); // 创建一个 SSL “上下文环境”（Context），用于配置和生成多个 SSL
    // 会话。

    SSL* ssl = SSL_new(
        ctx); // 为每一个TCP连接创建一个新的 SSL 对象（即一个“会话”对象）。
    SSL_set_fd(ssl,
        server_fd); // l告诉 `SSL` 对象：你要在哪个 socket 文件描述符上进行
    // TLS 通信。`SSL` 内部会将这个 fd 存起来，之后所有
    // `SSL_read()`、`SSL_write()` 都会用这个 fd 来传输数据。
    if (SSL_connect(ssl) != 1) // 开始 TLS 握手,之后使用SSL_write等收发加密数据
        ERR_print_errors_fp(stderr);
    // printf("SSL错误\n");

    SSL_CTX_free(ctx); // 清理上下文环境

    return ssl;
}

int httpsProxy(int client_fd) {
    // 解析客户端请求体
    http_request_t req = { 0 };
    if (parseHttpRequest(client_fd, &req) != 0)
        httpError(client_fd, "请求行解析错误!\n");

    // 现在先假设客户端是https请求
    int server_fd = openConnectfd(req.host, "443");

    // 读取客户端请求,发送给服务器
    rio_t rio;
    char buf[MAXBUF];
    rio_readinitb(&rio, client_fd);
    int n;

    SSL* ssl = init_ssl(server_fd);

    printf("收到客户端请求:\n");
    while ((n = rio_readlineb(&rio, buf, sizeof(buf))) > 0) {
        printf("%s\n", buf);
        SSL_write(ssl, buf, n);
    }

    // 读取服务器响应并返回给客户端
    int save_fd = open("www/save_file", O_WRONLY | O_CREAT | O_TRUNC);
    while ((n = SSL_read(ssl, buf, sizeof(buf))) > 0) {
        rio_written(client_fd, buf, n);
        write(save_fd, buf, n);
    }
    close(save_fd);

    SSL_shutdown(ssl); // 完成TLS的关闭通知握手
    SSL_free(ssl);     // 清理会话

    close(server_fd);

    return 0;
}
