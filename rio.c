#include "rio.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*
 * 使用方法同read()函数,从fd中读取n个字符放到usrbuf中
 * 成功时：返回实际读取的字节数（类型为 ssize_t，可能小于 count）。
 * 到达文件末尾（EOF）：返回 0。
 * 出错时：返回 -1，并设置 errno 表示错误类型（如 EAGAIN、EBADF）。
 */
ssize_t rio_readn(int fd, void* usrbuf, size_t n) {
    size_t nleft = n; // 动态跟踪剩余需读取的字节数，初始值为 n
    ssize_t nread;
    char* bufp = usrbuf;

    while (nleft > 0) {
        if ((nread = read(fd, bufp, nleft)) < 0) {
            if (errno == EINTR) // 如果被信号中断（errno == EINTR），将 nread
                // 置为 0 并继续循环，避免因信号导致读取失败
                nread = 0;
            else
                return -1;
        } else if (nread == 0)
            break; // EOF
        nleft -= nread;
        bufp += nread;
    }
    return (n - nleft);
}

/*
 * 从usrbuf中读取n个字符写入到fd中
 * 处理信号中断（EINTR）和部分写入（Partial
 * Write）场景,避免因系统调用中断导致数据不完整
 */
ssize_t rio_written(int fd, void* usrbuf, size_t n) {
    size_t nleft = n;
    ssize_t nwritten;
    char* bufp = usrbuf;

    while (nleft > 0) {
        if ((nwritten = write(fd, bufp, nleft)) <= 0) {
            if (errno == EINTR) // 如果被信号中断,尝试重新写入
                nwritten = 0;
            else
                return -1;
        } else if (
            nwritten ==
            0) // 理论上不应发生（写入0字节可能表示磁盘满等极端情况），视为错误返回
            // -1。
            return -1;
        nleft -= nwritten;
        bufp += nwritten;
    }
    return n;
}

/*
 * 初始化rio结构体
 */
void rio_readinitb(rio_t* rp, int fd) {
    rp->rio_fd = fd;
    rp->rio_buf_left_cnt = 0;
    rp->rio_buf_bptr = rp->rio_buf;
}

/*
 * 从rp中读取n个字符放到usrbuf中
 * 这是一个辅助函数,所以声明为static
 */
static ssize_t rio_read(rio_t* rp, char* usrbuf, size_t n) {
    int cnt;

    while (rp->rio_buf_left_cnt <= 0) { // 如果rio缓冲区空了
        rp->rio_buf_left_cnt =
            read(rp->rio_fd, rp->rio_buf, sizeof(rp->rio_buf)); // 填满缓冲区

        if (rp->rio_buf_left_cnt < 0) {
            if (errno !=
                EINTR) // 不是被中断信号打断,直接退出,如果是,进入下一次while循环,重新读
                return -1;
        } else if (rp->rio_buf_left_cnt == 0) // 如果fd读完了
            return 0;
        else
            rp->rio_buf_bptr = rp->rio_buf;
    }

    // 从rio缓冲区中复制min(n,rp->rio_buf_left_cnt)个字节到用户缓冲区中
    cnt = n;
    if (rp->rio_buf_left_cnt < n)
        cnt = rp->rio_buf_left_cnt;
    memcpy(
        usrbuf, rp->rio_buf_bptr,
        cnt); // 若cnt小于n,说明rio缓冲区中字符数没有n个了,需要从fd中再读取,这里没有在函数内进行,而是在外部调用时调节

    rp->rio_buf_bptr += cnt;
    rp->rio_buf_left_cnt -= cnt;
    return cnt;
}

/*
 * 从rp中读取n个字符放到usrbuf中
 */
ssize_t rio_readnb(rio_t* rp, void* usrbuf, size_t n) {
    size_t nleft = n;
    ssize_t nread;
    char* bufp = usrbuf;

    while (nleft > 0) {
        if ((nread = rio_read(rp, usrbuf, n)) < 0) {
            return -1;
        } else if (nread == 0)
            break;

        nleft -= nread;
        bufp += nread;
    }
    return (n - nleft);
}

/*
 * 行缓冲读取：从rio_t结构体中逐字符读取数据，直到遇到换行符 \n 或达到最大长度
 * line_len。 健壮性处理：兼容EOF（文件结束）、信号中断（EINTR，由rio_read
 * 内部处理）和错误场景。
 * 字符串安全：自动在末尾添加空字符\0，确保输出为合法C字符串。
 */
ssize_t rio_readlineb(rio_t* rp, void* usrbuf, size_t line_len) {
    int n, rc;
    char c, * bufp = usrbuf;

    for (n = 1; n < line_len; n++) {
        // 逐个字符读取
        if (((rc = rio_read(rp, &c, 1)) == 1)) {
            *bufp++ = c;
            if (c == '\n') {
                n++;
                break;
            }
        } else if (rc == 0) {
            if (n == 1)
                return 0; // EOF,没有数据被读取
            else
                break; // EOF,读取了一些数据
        } else
            return -1; // 出错了
    }

    *bufp = '\0';
    return (n - 1);
}