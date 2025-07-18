#ifndef _NET_UTILS_H
#define _NET_UTILS_H


int openListenfd(const char* port);
int acceptClientfd(int listen_fd);
int openConnectfd(const char* hostname, const char* port);

int make_socket_non_blocking(int fd);

#endif
