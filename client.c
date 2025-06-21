#define _POSIX_C_SOURCE 200122L
#include<stdio.h>
#include<netdb.h>
#include<unistd.h>

#include"net_utils.h"
#include"config.h"

#include<openssl/ssl.h>
#include<openssl/err.h>

int main(int argc, char const* argv[]) {

    if (!PROXY_MODE) {
        int listen_fd = openListenfd(PORT);
        listen(listen_fd, 5);

        while (1) {
            int client_fd = acceptClientfd(listen_fd);

            handleClient(client_fd, respondClient);

            close(client_fd);
        }
        close(listen_fd);
    } else {


        int server_fd = openConnectfd("www.bilibili.com", "443");

        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();

        SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
        SSL* ssl = SSL_new(ctx);
        SSL_set_fd(ssl, server_fd);
        if (SSL_connect(ssl) != 1) {
            ERR_print_errors_fp(stderr);
            return 1;
        }

#include<fcntl.h>
        int req_fd = open("www/req.txt", O_RDONLY);
        char buf[MAXBUF];
        int n;
        while ((n = read(req_fd, buf, sizeof(buf))) > 0) {
            SSL_write(ssl, buf, n);
        }
        close(req_fd);

        int save_fd = open("www/save.html", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        while ((n = SSL_read(ssl, buf, sizeof(buf))) > 0) {
            write(save_fd, buf, n);
        }
        close(save_fd);

        SSL_shutdown(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ctx);

        close(server_fd);
    }

    return 0;
}