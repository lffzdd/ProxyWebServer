#define _POSIX_C_SOURCE 200122L
#include "net_utils.h"
#include "config.h"
#include "http_respond.h"
#include "http_proxy.h"
#include "rio.h"
#include <netdb.h>
#include <stdio.h>
#include <string.h>

typedef struct addrinfo addrinfo;
typedef struct sockaddr sockaddr;
typedef struct sockaddr_storage sockaddr_storage;

int openListenfd(const char* port) {
    addrinfo hints = { 0 }, * results;
    // memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM; // 使用 TCP 连接

    getaddrinfo("localhost", port, &hints, &results);

    int listen_fd;
    for (struct addrinfo* p = results; p; p = p->ai_next) {
        if ((listen_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) <
            0)
            continue;

        int sockoptval = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,
            (const void*)&sockoptval, sizeof(sockoptval));

        if (bind(listen_fd, p->ai_addr, p->ai_addrlen) == 0)
            break;

        close(listen_fd);
    }
    return listen_fd;
}

int acceptClientfd(int listen_fd) {
    sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client_fd =
        accept(listen_fd, (sockaddr*)&client_addr, &client_addr_len);

    char hostname[256], port[128];
    getnameinfo((sockaddr*)&client_addr, client_addr_len, hostname,
        sizeof(hostname), port, sizeof(port), 0);
    printf("accept来自%s:%s的connect:\n", hostname, port);

    return client_fd;
}

int handleClient(int client_fd, client_handler_t handler) {
    return handler(client_fd);
}

int respondClient(int client_fd) {
    httpRespond(client_fd);
    char response_buf[] = "你好,客户端\n";
    rio_written(client_fd, response_buf, sizeof(response_buf));

    return 0;
}


int openConnectfd(const char* hostname, const char* port) {
    addrinfo hints = { 0 }, * results;

    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_family = AF_UNSPEC;     // IPV4和IPV6

    getaddrinfo(hostname, port, &hints, &results);

    int server_fd;
    for (addrinfo* p = results; p; p = p->ai_next) {
        if ((server_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue;
        if (connect(server_fd, p->ai_addr, p->ai_addrlen) == 0)
            break;

        close(server_fd);
    }

    return server_fd;
}

int proxyClient(int client_fd) {
    httpsProxy(client_fd);

    return 0;
}