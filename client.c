#include<stdio.h>
#include<arpa/inet.h>
int main() {
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in  server_addr ;
    server_addr.sin_family=AF_INET;
    server_addr.sin_addr=
}