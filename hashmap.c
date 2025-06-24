#include"hashmap.h"

int fd_hash_func(int key_fd) {
    return key_fd % FD_HASH_REMAINDER;
}


void remove_fd_pair(fd_map_t fd_map[FD_HASH_SIZE], int fd) { }/// @brief 插入一对映射
void add_fd_pair(fd_map_t* fd_map[FD_HASH_SIZE], int client_fd, int server_fd) {
    int client_idx = fd_hash_func(client_fd);
    int server_idx = fd_hash_func(server_fd);

    fd_peer_t client_peer = { SERVER_FD,server_fd };
    fd_peer_t server_peer = { CLIENT_FD,client_fd };

    fd_map_t* client_node = malloc(sizeof(fd_map_t));
    client_node->hash_key_fd = client_fd;
    client_node->fd_peer = client_peer;
    client_node->next = fd_map[client_idx];
    fd_map[client_idx] = client_node;

    fd_map_t* server_node = malloc(sizeof(fd_map_t));
    server_node->hash_key_fd = server_fd;
    server_node->fd_peer = server_peer;
    server_node->next = fd_map[server_idx];
    fd_map[server_idx] = server_node;
}

fd_peer_t get_peer_fd(fd_map_t* fd_map[FD_HASH_SIZE], int fd) {
    int idx = fd_hash_func(fd);

    fd_map_t* map_node = fd_map[idx];
    while (map_node) {
        if (map_node->hash_key_fd == fd)
            return map_node->fd_peer;

        map_node = map_node->next;
    }

    fd_peer_t peer_null = { CLIENT_FD,FD_HASH_NULL };
    return peer_null;
}

void remove_fd_pair(fd_map_t* fd_map[FD_HASH_SIZE], int fd) {
    int idx = fd_hash_func(fd);
    fd_map_t** node_pp = &fd_map[idx];
    while (*node_pp) {
        if ((*node_pp)->hash_key_fd == fd) {
            fd_map_t* tmp = *node_pp;
            *node_pp = (*node_pp)->next;
            free(tmp);
            break;;
        }
        node_pp = &((*node_pp)->next);
    }

    int peer_fd = (get_peer_fd(fd_map, fd)).fd;
    int peer_idx = fd_hash_func(peer_fd);
    fd_map_t** node_pp = &fd_map[peer_idx];
    while (*node_pp) {
        if ((*node_pp)->hash_key_fd == peer_fd) {
            fd_map_t* tmp = *node_pp;
            *node_pp = (*node_pp)->next;
            free(tmp);
            return;
        }
        node_pp = &((*node_pp)->next);
    }
}
