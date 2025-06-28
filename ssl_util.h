#ifndef SSL_UTIL_H
#define SSL_UTIL_H

#include <openssl/ssl.h>
#include <openssl/err.h>

// SSL 初始化和清理
int init_ssl();
void cleanup_ssl();

// SSL 连接相关函数
SSL_CTX* create_ssl_client_context();
SSL* create_ssl_connection(int fd, SSL_CTX* ctx);
int ssl_connect_to_server(SSL* ssl);
void cleanup_ssl_connection(SSL* ssl);
void cleanup_ssl_context(SSL_CTX* ctx);

// SSL 读写操作
int ssl_read_data(SSL* ssl, char* buffer, int size);
int ssl_write_data(SSL* ssl, const char* buffer, int size);

#endif // SSL_UTIL_H