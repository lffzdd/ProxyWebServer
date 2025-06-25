#include "http_proxy.h"
#include "config.h"
#include "http_util.h"
#include "net_utils.h"
#include "rio.h"
#include <fcntl.h>
#include"conn_state_machine.h"


int httpsProxy(int client_fd) {
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
