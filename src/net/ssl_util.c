#include "net/ssl_util.h"
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

int init_ssl() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    return 0;
}

void cleanup_ssl() {
    EVP_cleanup();
    ERR_free_strings();
}

SSL_CTX* create_ssl_client_context() {
    const SSL_METHOD* method = TLS_client_method();
    SSL_CTX* ctx = SSL_CTX_new(method);

    if (!ctx) {
        ERR_print_errors_fp(stderr);
        return NULL;
    }

    // 禁用证书验证（用于代理）
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);

    return ctx;
}

SSL* create_ssl_connection(int fd, SSL_CTX* ctx) {
    SSL* ssl = SSL_new(ctx);
    if (!ssl) {
        ERR_print_errors_fp(stderr);
        return NULL;
    }

    SSL_set_fd(ssl, fd);
    return ssl;
}

int ssl_connect_to_server(SSL* ssl) {
    int ret = SSL_connect(ssl);
    if (ret <= 0) {
        int err = SSL_get_error(ssl, ret);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            return 0; // 需要重试
        }
        ERR_print_errors_fp(stderr);
        return -1;
    }
    return 1; // 成功
}

void cleanup_ssl_connection(SSL* ssl) {
    if (ssl) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
    }
}

void cleanup_ssl_context(SSL_CTX* ctx) {
    if (ctx) {
        SSL_CTX_free(ctx);
    }
}

int ssl_read_data(SSL* ssl, char* buffer, int size) {
    int n = SSL_read(ssl, buffer, size);
    if (n <= 0) {
        int err = SSL_get_error(ssl, n);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            errno = EAGAIN;
            return -1;
        }
        if (err == SSL_ERROR_ZERO_RETURN) {
            return 0; // SSL 连接正常关闭
        }
        return -1; // 错误
    }
    return n;
}

int ssl_write_data(SSL* ssl, const char* buffer, int size) {
    int n = SSL_write(ssl, buffer, size);
    if (n <= 0) {
        int err = SSL_get_error(ssl, n);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            errno = EAGAIN;
            return -1;
        }
        return -1; // 错误
    }
    return n;
}