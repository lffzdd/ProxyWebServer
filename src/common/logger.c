#include "common/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// 默认日志级别
log_level_t g_log_level = LOG_INFO;

// 日志级别名称
static const char* level_names[] = {
    "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

// 日志级别对应的颜色（ANSI转义序列）
static const char* level_colors[] = {
    "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"
};

void log_set_level(log_level_t level) {
    g_log_level = level;
}

void log_message(log_level_t level, const char* file, int line, const char* fmt, ...) {
    // 只输出大于等于设置级别的日志
    if (level < g_log_level) {
        return;
    }

    // 获取当前时间
    time_t t = time(NULL);
    struct tm* lt = localtime(&t);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", lt);

    // 获取基本文件名（不带路径）
    const char* filename = strrchr(file, '/');
    if (filename) {
        filename++; // 跳过'/'字符
    } else {
        filename = file;
    }

    // 打印日志头
    fprintf(stderr, "%s [%s%5s\x1b[0m] %s:%d: ", 
            time_str, level_colors[level], level_names[level], filename, line);

    // 打印日志内容
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    // 如果日志消息末尾没有换行符，添加一个
    if (fmt[strlen(fmt) - 1] != '\n') {
        fprintf(stderr, "\n");
    }

    // 如果是致命错误，立即退出程序
    if (level == LOG_FATAL) {
        exit(EXIT_FAILURE);
    }
}
