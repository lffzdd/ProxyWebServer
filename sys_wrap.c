#include "sys_wrap.h"
#include <fcntl.h>
#include <malloc.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include<unistd.h>
#include <sys/epoll.h>

int Open(const char* file, int flag) {
    int fd = open(file, flag);
    if (fd < 0)
        perror("open");

    return fd;
}

int Socket(int domain, int type, int protocol) {
    int fd = socket(domain, type, protocol);
    if (fd < 0)
        perror("socket");

    return fd;
}

int Listen(int fd, int n) {
    int ret = listen(fd, n);
    if (ret < 0)
        perror("listen");

    return ret;
}
int Accept(int fd, struct sockaddr* addr, socklen_t* addr_len) {
    int client_fd = accept(fd, addr, addr_len);
    if (client_fd < 0)
        perror("accept");

    return client_fd;
}
void* Malloc(size_t size) {
    void* p = malloc(size);
    if (p == NULL)
        perror("malloc");

    return p;
}

int Read(int fd, void* buf, size_t nbytes) {
    int n = read(fd, buf, nbytes);
    if (n < 0)
        perror("read");

    return n;
}

int Write(int fd, const void* buf, size_t nbytes) {
    int n = write(fd, buf, nbytes);
    if (n < 0)
        perror("write");

    return n;
}

int Epoll_ctl(int epfd, int op, int fd, struct epoll_event* ev) {
    int ret = epoll_ctl(epfd, op, fd, ev);
    if (ret < 0)
        perror("epoll_ctl");

    return ret;
}
