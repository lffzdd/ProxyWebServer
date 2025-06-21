#include"http_proxy.h"
#include"http_util.h"
#include"net_utils.h"
#include"config.h"
#include"rio.h"
#include<openssl/ssl.h>
#include<openssl/err.h>
#include<fcntl.h>

int httpProxy(int client_fd) {
    // 解析客户端请求体
    http_request_t req = { 0 };
    if (parseHttpRequest(client_fd, &req) != 0)
        httpError(client_fd, "请求行解析错误!\n");


    // 现在先假设客户端是https请求
    int server_fd = openConnectfd(req.host, "443");

    //读取客户端请求,发送给服务器
    rio_t rio;
    char buf[MAXBUF];
    rio_readinitb(&rio, client_fd);
    int n;

    SSL* ssl = init_ssl(server_fd);

    while ((n = rio_readlineb(&rio, buf, sizeof(buf))) > 0) {
        SSL_write(ssl, buf, n);
    }

    // 读取服务器响应并返回给客户端
    int save_fd = open("www/save_file", O_WRONLY | O_CREAT | O_TRUNC);
    while ((n = SSL_read(ssl, buf, sizeof(buf)))) {
          
    }



    return 0;
}

inline SSL* init_ssl(int server_fd) {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, server_fd);
    if (SSL_connect(ssl) != 1)
        ERR_print_errors_fp(stderr);

    return ssl;
}