#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <signal.h>
#include <unistd.h>

#include "common/config.h"
#include "common/logger.h"
#include "proxy/conn_state_machine.h"
#include "http/http_util.h"
#include "net/net_utils.h"
#include "common/rio.h"
#include "common/sys_wrap.h"

// 全局变量，用于信号处理
static int g_listen_fd = -1;
static int g_epfd = -1;
static volatile int g_running = 1;

// 信号处理函数
void signal_handler(int signum) {
    LOG_INFO("捕获信号 %d，准备关闭服务器", signum);
    g_running = 0;
}

// 初始化信号处理
void setup_signal_handlers() {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    // 处理SIGINT (Ctrl+C)
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        LOG_ERROR("无法设置SIGINT处理器: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // 处理SIGTERM
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        LOG_ERROR("无法设置SIGTERM处理器: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

int main() {
    // 初始化日志
    log_set_level(LOG_INFO);

    // 设置信号处理
    setup_signal_handlers();

    // 监听客户端
    g_listen_fd = openListenfd(PORT);
    if (g_listen_fd < 0) {
        LOG_FATAL("无法打开监听端口 %s: %s", PORT, strerror(errno));
        exit(EXIT_FAILURE);
    }

    LOG_INFO("代理服务器启动，监听端口: %s，listen_fd = %d", PORT, g_listen_fd);

    Listen(g_listen_fd, EPOLL_LEN);

    g_epfd = epoll_create1(0);
    if (g_epfd < 0) {
        LOG_FATAL("epoll_create1 失败: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    struct epoll_event events[EPOLL_LEN * 2];

    struct epoll_event ev = { .events = EPOLLIN, .data.ptr = NULL };
    Epoll_ctl(g_epfd, EPOLL_CTL_ADD, g_listen_fd, &ev);

    LOG_INFO("事件循环开始，等待连接...");

    while (g_running) {
        int nready = epoll_wait(g_epfd, events, EPOLL_LEN * 2, 1000); // 1秒超时以便检查g_running

        if (nready < 0) {
            if (errno == EINTR) {
                // 被信号中断，检查g_running
                continue;
            }
            LOG_ERROR("epoll_wait 错误: %s", strerror(errno));
            break;
        }

        for (int i = 0; i < nready; i++) {
            fd_event_t* fd_event = events[i].data.ptr;

            if (fd_event == NULL) { // 新连接到来
                LOG_INFO("检测到新连接");
                add_client_to_epoll(g_epfd, g_listen_fd);
            } else { // 已有连接
                if (handle_connection_state(fd_event, g_epfd) != 0)
                    handle_connection_state(fd_event, g_epfd); // 处理CONN_ERROR
            }
        }
    }

    LOG_INFO("服务器正在关闭...");
    close(g_epfd);
    close(g_listen_fd);
    LOG_INFO("服务器已关闭");

    return 0;
}
