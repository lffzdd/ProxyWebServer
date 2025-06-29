CC = gcc
CFLAGS = -Wall -Wextra -I./include -g
LDFLAGS = -lssl -lcrypto -lpthread

# 源文件目录
SRC_DIR = src
COMMON_SRC = $(wildcard $(SRC_DIR)/common/*.c)
NET_SRC = $(wildcard $(SRC_DIR)/net/*.c)
HTTP_SRC = $(wildcard $(SRC_DIR)/http/*.c)
PROXY_SRC = $(wildcard $(SRC_DIR)/proxy/*.c)

# 目标文件
COMMON_OBJ = $(COMMON_SRC:.c=.o)
NET_OBJ = $(NET_SRC:.c=.o)
HTTP_OBJ = $(HTTP_SRC:.c=.o)
PROXY_OBJ = $(PROXY_SRC:.c=.o)

# 应用程序
PROXY_APP = proxy
CLIENT_APP = client
SERVER_APP = server

# 所有目标
all: $(PROXY_APP) $(CLIENT_APP) $(SERVER_APP)

# 代理服务器
$(PROXY_APP): apps/proxy/proxy.o $(COMMON_OBJ) $(NET_OBJ) $(HTTP_OBJ) $(PROXY_OBJ)
	$(CC) $^ -o $@ $(LDFLAGS)

# 主程序
main: main.o $(COMMON_OBJ) $(NET_OBJ) $(HTTP_OBJ) $(PROXY_OBJ)
	$(CC) $^ -o $@ $(LDFLAGS)

# 客户端
$(CLIENT_APP): apps/client/client.o $(COMMON_OBJ) $(NET_OBJ)
	$(CC) $^ -o $@ $(LDFLAGS)

# 服务器
$(SERVER_APP): apps/server/server.o $(COMMON_OBJ) $(NET_OBJ) $(HTTP_OBJ)
	$(CC) $^ -o $@ $(LDFLAGS)

# 编译规则
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理
clean:
	rm -f $(SRC_DIR)/*/*.o apps/*/*.o *.o $(PROXY_APP) $(CLIENT_APP) $(SERVER_APP) main

.PHONY: all clean
