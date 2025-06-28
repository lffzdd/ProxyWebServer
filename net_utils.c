#define _POSIX_C_SOURCE 200122L
#include "net_utils.h"
#include "config.h"
#include "http_proxy.h"
#include "rio.h"
#include "sys_wrap.h"
#include <fcntl.h>
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

    int ret = getaddrinfo(NULL, port, &hints, &results);
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        return -1;
    }

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

    freeaddrinfo(results);

    return listen_fd;
}

int acceptClientfd(int listen_fd) {
    sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client_fd =
        Accept(listen_fd, (sockaddr*)&client_addr, &client_addr_len);

    char hostname[256], port[128];
    if (getnameinfo((sockaddr*)&client_addr, client_addr_len, hostname,
        sizeof(hostname), port, sizeof(port), 0) == 0)
        printf("accept来自%s:%s的connect:\n", hostname, port);
    else
        printf("accept: unknown client\n");

    make_socket_non_blocking(client_fd);

    return client_fd;
}

int openConnectfd(const char* hostname, const char* port) {
    addrinfo hints = { 0 }, * results;

    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_family = AF_UNSPEC;     // IPV4和IPV6

    int ret = getaddrinfo(hostname, port, &hints, &results);
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo failed for %s:%s - %s\n", hostname, port, gai_strerror(ret));
        return -1;
    }

    int server_fd = -1;
    for (addrinfo* p = results; p; p = p->ai_next) {
        if ((server_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
            perror("socket");
            continue;
        }

        if (connect(server_fd, p->ai_addr, p->ai_addrlen) == 0) {
            printf("成功连接到 %s:%s\n", hostname, port);
            break;
        }

        perror("connect");
        close(server_fd);
        server_fd = -1;
    }

    freeaddrinfo(results);

    if (server_fd > 0) {
        make_socket_non_blocking(server_fd);
    }

    return server_fd;
}

int proxyClient(int client_fd) {
    // httpsProxy(client_fd);

    return 0;
}

int make_socket_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);

    return fcntl(fd, F_SETFL, flags |= O_NONBLOCK);
}
