#include<unistd.h>
#include<fcntl.h>
#include<string.h>
#include "http_respond.h"
#include"http_util.h"
#include"config.h"
#include"rio.h"

int handle_get_method(int client_fd, const http_request_t* req) {
    const char* real_uri = req->uri[0] == '/' ? req->uri + 1 : req->uri; // 确保根路径后只有1个/
    char filepath[1024];
    // /index.html?a=1&b=2 -> ROOTDIR/index.html?a=1&b=2

    if (real_uri[0] == '\0')
        snprintf(filepath, sizeof(filepath), "%s/%s", RESPOND_ROOTDIR, DEFAULT_HOME_PAGE);
    else
        snprintf(filepath, sizeof(filepath), "%s/%s", RESPOND_ROOTDIR, real_uri);

    // 检查文件是否存在
    if (access(filepath, F_OK) != 0) {
        httpError(client_fd, "请求文件不存在!");
        return 1;
    }

    if (access(filepath, F_OK | X_OK) != 0) { // 文件存在但不可执行,静态资源
        // 打开文件
        const char* mimetype = getMimeType(filepath); // 因为filepath和filename都只需要分辨后缀,所以直接传filepath也可以
        int fd = open(filepath, O_RDONLY);
        if (fd < 0)
            httpError(client_fd, "打开文件失败!");

        // 读取文件并发送
        rio_t rio;
        rio_readinitb(&rio, client_fd);
        char buf[MAXBUF];

        // 先发送响应头
        if (snprintf(buf, sizeof(buf), "HTTP/2 200 OK\r\nContent-Type: %s\r\n\r\n", mimetype)) {
            rio_written(client_fd, buf, strlen(buf));
        } else {
            httpError(client_fd, "发送响应头失败!");
            return 1;
        }

        ssize_t char_cnt;
        while ((char_cnt = read(fd, buf, sizeof(buf))) > 0) {
            if (rio_written(client_fd, buf, char_cnt) != char_cnt) {
                httpError(client_fd, "传输文件失败!");
                return 1;
            }
        }

        return 0;

    } else { // 文件存在且可执行,动态响应

    }
}
int httpRespond(int client_fd) {
    http_request_t req = { 0 };

    // 解析请求
    if (parseHttpRequest(client_fd, &req) != 0) {
        httpError(client_fd, "请求头解析失败!\n");
        return 1;
    }


    // 实现请求方法
    switch (req.method) {
        case UNSUPPORTED:
            httpError(client_fd, "不支持的方法!");
            return 1;
        case GET:
            handle_get_method(client_fd, &req);
            break;
        default:
            break;
    }

    char response_buf[] = "你好,客户端\n";
    rio_written(client_fd, response_buf, sizeof(response_buf));

    return 0;
}