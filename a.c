#include "http_proxy.h"
#include "config.h"
#include "http_util.h"
#include "net_utils.h"
#include "rio.h"
#include <fcntl.h>
#include<sys/epoll.h>


typedef struct{
    int client_fd;
    int server_fd;
}socket_fd_pair_t;

#define EPOLL_LEN 1024
int main() {
    // 监听客户端
    int listen_fd = openListenfd(PORT);
    int epfd = epoll_create1(0);
    struct epoll_event events[EPOLL_LEN], ev;
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);

    socket_fd_pair_t fd_pairs[EPOLL_LEN];
    while (1) {
        int nready = epoll_wait(epfd, events, EPOLL_LEN, -1);
        for (int i = 0; i < nready; i++) {
            int fd = events[i].data.fd;

            if (fd == listen_fd) { // 如果listen到了新的client
                int client_fd = acceptClientfd(listen_fd);

                ev.events = EPOLLIN;
                ev.data.fd = client_fd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev);

                // 第一次的时候要为client连接server
                http_request_t req = { 0 };
                parseHttpRequest(fd, &req);

                int server_fd;
                if (req.method == CONNECT)
                    server_fd = openConnectfd(req.host, "443");
                else
                    server_fd = openConnectfd(req.host, "80");

                ev.events = EPOLLIN;
                ev.data.fd = server_fd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev);

                socket_fd_pair_t fd_pair = { client_fd,server_fd };
                
            } else { // 如果是已存在的client或server
                // 不管是client还是server,读到了再转发数据
                rio_t rio;


            }

        }


    }


    // 解析客户端请求体
    http_request_t req = { 0 };
    if (parseHttpRequest(client_fd, &req) != 0)
        httpError(client_fd, "请求行解析错误!\n");

    int server_fd;
    if (req.method == CONNECT)
        server_fd = openConnectfd(req.host, "443");
    else
        server_fd = openConnectfd(req.host, "80");

    // 读取客户端请求,发送给服务器
    rio_t client_rio, server_rio;
    char buf[MAXBUF];
    rio_readinitb(&client_rio, client_fd);
    rio_readinitb(&server_rio, server_fd);
    int n;

    while (1) {
        // printf("收到客户端请求:\n");
        while ((n = rio_readlineb(&client_rio, buf, sizeof(buf))) >
            0) {                       // 读取 client
            rio_written(server_fd, buf, n); // 发送到 server
        }

        // 读取服务器响应并返回给客户端
        // int save_fd = open("www/save_file", O_WRONLY | O_CREAT | O_TRUNC);
        while ((n = rio_readlineb(&server_rio, buf, sizeof(buf))) >
            0) {                       // 读取server
            rio_written(client_fd, buf, n); // 发送到 client
        }
    }
    close(server_fd);

    return 0;
}
