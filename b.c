#define _POSIX_C_SOURCE 200122L
#include<stdio.h>
#include<stdlib.h>
#include<netdb.h>
#include<string.h>
#include"rio.h"

#define LEN 1024
#define PORT "8888"
typedef struct addrinfo addrinfo;
typedef struct sockaddr sockaddr;
int main() {
    int listen_fd, client_fd;

    addrinfo  hints, * results;
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    getaddrinfo("localhost", PORT, &hints, &results);
    for (addrinfo* p = results; p; p = p->ai_next) {
        listen_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listen_fd < 0)
            continue;

        int sockoptval = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (const void*)&sockoptval, sizeof(sockoptval));

        if (bind(listen_fd, p->ai_addr, p->ai_addrlen) == 0)
            break;

        close(listen_fd);
    }

    listen(listen_fd, 5);

    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    client_fd = accept(listen_fd, (sockaddr*)&client_addr, &client_addr_len);

    char client_hostname[LEN], client_port[LEN];
    getnameinfo((sockaddr*)&client_addr, client_addr_len, client_hostname, sizeof(client_hostname), client_port, sizeof(client_port), 0);
    printf("接受到来自%s:%s的connect\n", client_hostname, client_port);

    printf("接受到请求:\n");
    rio_t rio;
    char request_buf[LEN];
    rio_readinitb(&rio, client_fd);
    while (rio_readlineb(&rio, request_buf, LEN) > 0) {
        printf("%s", request_buf);
        if (strcmp(request_buf, "\n") == 0 || strcmp(request_buf, "\r\n") == 0)
            break;
    }
    char response[] = "发送响应!\n";
    rio_written(client_fd, response, sizeof(response));

    close(client_fd);
    close(listen_fd);

    return 0;
}