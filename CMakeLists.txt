cmake_minimum_required(VERSION 3.10)  # 指定CMake最低版本要求
project(ProxyWebServer C)

# 设置C语言标准为C11
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 包含头文件目录
include_directories(${CMAKE_SOURCE_DIR}/include)

# 设置编译选项
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -Wextra -g -O0")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")

# 查找并添加依赖库(如OpenSSL)
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

# 收集源文件
file(GLOB_RECURSE SRC_COMMON "src/common/*.c")
file(GLOB_RECURSE SRC_NET "src/net/*.c")
file(GLOB_RECURSE SRC_HTTP "src/http/*.c")
file(GLOB_RECURSE SRC_PROXY "src/proxy/*.c")

# 创建静态库
add_library(common STATIC ${SRC_COMMON})
add_library(net STATIC ${SRC_NET})
add_library(http STATIC ${SRC_HTTP})
add_library(proxy STATIC ${SRC_PROXY})

# 创建可执行文件并链接所需库
add_executable(proxy_app apps/proxy/proxy.c)
target_link_libraries(proxy_app proxy http net common ${OPENSSL_LIBRARIES} pthread)

# 客户端应用
add_executable(client_app apps/client/client.c)
target_link_libraries(client_app http net common ${OPENSSL_LIBRARIES})

# 服务器应用
add_executable(server_app apps/server/server.c)
target_link_libraries(server_app  http net common ${OPENSSL_LIBRARIES} pthread)
