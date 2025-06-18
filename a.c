#define _POSIX_C_SOURCE 200112L
#include<stdio.h>
#include<string.h>
#include<unistd.h>
#include<netdb.h>
#include"rio.h"

#define PORT "8888"


int main(int argc, char* argv[]) {
    int listen_fd;
    int client_fd;

    // 1. 绑定套接字和本机地址
    struct addrinfo hints, * result;
    memset(&hints, 0, sizeof(hints));

    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;// 选择后续监听的地址
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    getaddrinfo("localhost", PORT, &hints, &result);

    for (struct addrinfo* p = result; p; p = p->ai_next) {
        listen_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

        if (listen_fd < 0) // 套接字创建失败,该本机地址不可用,继续尝试下一个
            continue;

        /* Eliminates "Address already in use" error from bind */
        int sockoptval = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (const void*)&sockoptval, sizeof(sockoptval));

        if (bind(listen_fd, p->ai_addr, p->ai_addrlen) == 0) {//绑定成功
            char host[1024], port[256];
            getnameinfo(p->ai_addr, p->ai_addrlen, host, sizeof(host), port, sizeof(port), 0);
            printf("本机地址%s:%s成功进行了绑定!\n", host, port);
            break;
        }

        close(listen_fd); //如果绑定失败,关闭该套接字,继续尝试下一个
    }

    freeaddrinfo(result);

    // 2. 监听连接
    listen(listen_fd, 5);
    printf("正在监听%s端口\n", PORT);

    // 3. 接受连接
    struct sockaddr_storage client_addr;
    socklen_t clientaddr_len = sizeof(client_addr);
    client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &clientaddr_len);

    char client_hostname[1024], client_port[256];
    getnameinfo((struct sockaddr*)&client_addr, clientaddr_len, client_hostname, sizeof(client_hostname), client_port, sizeof(client_port), 0);
    printf("接受来自%s:%s的连接\n", client_hostname, client_port);

    // 4. 处理连接
    char request_buf[8192];
    memset(request_buf, 0, sizeof(request_buf));
    printf("接受到请求:\n");

    rio_t rio;
    rio_readinitb(&rio, client_fd);
    size_t read_cnt;
    while ((read_cnt = rio_readlineb(&rio, request_buf, 8192)) != 0) {
        printf("%s\n", request_buf);
        // 检测到空行就结束
        if (strcmp(request_buf, "\r\n") == 0 || strcmp(request_buf, "\n") == 0)
            break;
    }

    close(client_fd);
    close(listen_fd);

    return 0;
}
