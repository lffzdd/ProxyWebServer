#ifndef _NET_UTILS_H
#define _NET_UTILS_H


int openListenfd(const char* port);
int acceptClientfd(int listen_fd);

int openConnectfd(const char* hostname, const char* port);

typedef int(*client_handler_t)(int client_fd);

int handleClient(int client_fd, client_handler_t handler);
int respondClient(int client_fd);
int proxyClient(int client_fd);

int testConnect(int server_fd);
#endif