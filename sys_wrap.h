#ifndef SYS_WRAP_H
#define SYS_WRAP_H

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include<sys/epoll.h>
#include<netdb.h>

#define CHECK_CALL_ERR(call)                                                   \
    do {                                                                       \
        if ((call) < 0) {                                                      \
            fprintf(stderr, "[%s -> %s] %s failed: %s\n", __FILE__, __func__,  \
                    #call, strerror(errno));                                   \
            return -1;                                                          \
        }                                                                      \
    } while (0)

int Open(const char* file, int flag);

int Socket(int domain, int type, int protocol);

int Listen(int fd, int n);

int Accept(int fd, struct sockaddr* addr, socklen_t* addr_len);

void* Malloc(size_t size);

int Read(int fd, void* buf, size_t nbytes);

int Epoll_ctl(int epfd, int op, int fd, struct epoll_event* ev);


#endif // SYS_WRAP_H
