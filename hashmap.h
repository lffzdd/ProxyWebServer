#ifndef HASH_MAP_H
#define HASH_MAP_H
/* 通过HASH表,建立client-server的socket隧道 */
#define FD_HASH_SIZE 4096
#define FD_HASH_REMAINDER 4093
#define FD_HASH_NULL -1

typedef enum { CLIENT_FD, SERVER_FD } fd_type_t;

typedef struct {
    fd_type_t socket_type; // 是client还是server
    int fd;                    // 对端fd
} fd_peer_t;

typedef struct fd_map_node {
    int hash_key_fd;// 当前节点代表哪个fd
    fd_peer_t fd_peer;// 其对端信息
    struct fd_map_node* next; // 拉链法 
} fd_map_t;

int fd_hash_func(int key_fd);

void add_fd_pair(fd_map_t fd_map[FD_HASH_SIZE], int client_fd, int server_fd);

fd_peer_t get_peer_fd(fd_map_t fd_map[FD_HASH_SIZE], int fd);

void remove_fd_pair(fd_map_t fd_map[FD_HASH_SIZE], int fd);
#endif // HASH_MAP_H