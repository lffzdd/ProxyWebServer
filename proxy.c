#define _POSIX_C_SOURCE 200122L
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>

#include "config.h"
#include "net_utils.h"

#include <openssl/err.h>
#include <openssl/ssl.h>

int main(int argc, char const* argv[]) {

    int listen_fd = openListenfd(PORT);
    listen(listen_fd, 5);

    while (1) {
        int client_fd = acceptClientfd(listen_fd);

        if (PROXY_MODE) {
            proxyClient(client_fd);
        }
        else {
            respondClient(client_fd);
        }

        close(listen_fd);
    }

    return 0;
}
