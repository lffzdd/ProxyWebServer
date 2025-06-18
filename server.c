#include<stdio.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>

int main() {
    // 1. 创建套接字
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    // 2.绑定地址和端口
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8888);
    bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    // 3.监听连接
    listen(server_fd, 5); //最多等待5个客户端
    printf("Server listening on port 8888\n");

    // 4.接受连接
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    printf("Client connected!\n");

    // 5.接收数据
    char buf[8192];
    memset(buf, 0, sizeof(buf));
    recv(client_fd, buf, sizeof(buf), 0);
    printf("Received from client:%s\n", buf);

    // 6.回复数据
    char response[] = "你好\n";
    send(client_fd, response, sizeof(response), 0);

    close(server_fd);
    close(client_fd);

    return 0;
}