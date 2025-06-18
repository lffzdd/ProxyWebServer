#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#define RIO_BUFSIZE 8192
typedef struct {
    int rio_fd;                // 文件描述符
    int rio_buf_left_cnt;      // 缓冲区中未读字节数
    char *rio_buf_bptr;        // 指向缓存区中下一个未读字符
    char rio_buf[RIO_BUFSIZE]; // 缓冲区
} rio_t;

ssize_t rio_readn(int fd, void *usrbuf, size_t n);
ssize_t rio_written(int fd, void *usrbuf, size_t n);
void rio_readinitb(rio_t *rp, int fd);
ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n);
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t line_len);
