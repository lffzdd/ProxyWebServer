#include "http/http_util.h"
#include "common/config.h"
#include "common/rio.h"
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

void httpError(int client_fd, const char* message) {
    rio_written(client_fd, (void*)message, strlen(message));
}


http_method_t parse_http_method(const char* method) {
    if ((strcmp(method, "CONNECT")) == 0)
        return CONNECT;
    if ((strcmp(method, "GET")) == 0)
        return GET;
    if ((strcmp(method, "HEAD")) == 0)
        return HEAD;

    return UNSUPPORTED;
}

int parseHttpRequest(int client_fd, http_request_t* req) {
    rio_t rio;
    char request_buf[MAXBUF];
    rio_readinitb(&rio, client_fd);

    char method[64], http_version[64]; // http_version记录 "HTTP/xxx"

    if ((rio_readlineb(&rio, request_buf, sizeof(request_buf))) > 0) {
        // 解析请求行
        sscanf(request_buf, "%s %s %s", method, req->uri, http_version);
        sscanf(http_version, "%[^/]/%s", req->proto, req->proto_ver);

        if (strcmp(req->proto, "HTTP") != 0) {
            httpError(client_fd, "非HTTP请求!");
            return 1;
        }

        req->method = parse_http_method(method);
    }

    // 处理后续请求头字段
    while ((rio_readlineb(&rio, request_buf, sizeof(request_buf))) > 0) {
        if (strcmp(request_buf, "\n") == 0 || strcmp(request_buf, "\r\n") == 0)
            break;
        if (sscanf(request_buf, "Host: %1023s", req->host) == 1)
            continue;
        if (sscanf(request_buf, "User-Agent: %511[^\r\n]", req->user_agent) ==
            1)
            continue;
        if (sscanf(request_buf, "Connection: %31s", req->connection) == 1)
            continue;
    }

    return 0;
}


const mime_map_t mime_map[] = {
    {".html", "text/html"},
    {".htm", "text/html"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".png", "image/png"},
    {".css", "text/css"},
    {".js", "application/javascript"},
    {".gif", "image/gif"},
    {".txt", "text/plain"},
    {NULL, "application/octet-stream"} // 结尾标志
};

/// @brief 根据文件后缀返回对应的 MIME 类型
mime_t getMimeType(const char* filepath) {
    const char* ext = strrchr(filepath, '.');
    if (ext == NULL)
        return mime_map[sizeof(mime_map) / sizeof(mime_map[0]) - 1].mime_type;

    for (int i = 0; mime_map[i].ext != NULL; i++) {
        if ((strcmp(ext, mime_map[i].ext)) == 0)
            return mime_map[i].mime_type;
    }

    return mime_map[sizeof(mime_map) / sizeof(mime_map[0]) - 1].mime_type;
}


