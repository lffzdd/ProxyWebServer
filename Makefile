CC = gcc
CFLAGS = -Wall -Wextra -g -std=c99 -D_GNU_SOURCE
LDFLAGS = 

# 源文件
SRCS = main.c conn_state_machine.c http_util.c net_utils.c rio.c sys_wrap.c
OBJS = $(SRCS:.c=.o)

# 目标程序
TARGET = proxy

# 默认目标
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

# 编译规则
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理
clean:
	rm -f $(OBJS) $(TARGET)

# 重新编译
rebuild: clean all

# 依赖关系
main.o: main.c config.h conn_state_machine.h
conn_state_machine.o: conn_state_machine.c conn_state_machine.h http_util.h net_utils.h sys_wrap.h
http_util.o: http_util.c http_util.h config.h rio.h
net_utils.o: net_utils.c net_utils.h config.h sys_wrap.h
rio.o: rio.c rio.h
sys_wrap.o: sys_wrap.c sys_wrap.h

.PHONY: all clean rebuild