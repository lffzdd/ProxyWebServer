# ProxyWebServer

一个用C语言开发的高性能代理Web服务器，支持HTTP/HTTPS协议代理，并提供客户端和服务器组件。

## 功能特性

- HTTP/HTTPS代理服务
- 多线程处理并发连接
- 支持SSL/TLS加密通信
- 提供简易Web客户端
- 包含测试用Web服务器

## 系统要求

- C编译器 (支持C11标准)
- CMake 3.10+
- OpenSSL库
- POSIX兼容系统 (Linux, macOS等)

## 安装

### 依赖安装

#### Ubuntu/Debian
```bash
sudo apt update
sudo apt install build-essential cmake libssl-dev
```

#### macOS
```bash
brew install cmake openssl
```

### 构建项目

1. 克隆项目
```bash
git clone https://github.com/yourusername/ProxyWebServer.git
cd ProxyWebServer
```

2. 创建并进入构建目录
```bash
mkdir -p build && cd build
```

3. 配置和构建项目
```bash
cmake ..
make
```

## 使用方法

构建完成后，项目会生成以下可执行文件：

### 代理服务器
```bash
./proxy_app [配置选项]
```

### Web客户端
```bash
./client_app [URL]
```

### 测试服务器
```bash
./server_app [端口]
```

## 项目结构

```
ProxyWebServer/
├── apps/               # 应用程序源码
│   ├── client/         # 客户端应用
│   ├── proxy/          # 代理服务器应用
│   └── server/         # 服务器应用
├── include/            # 头文件
├── src/                # 源代码
│   ├── common/         # 通用工具函数
│   ├── http/           # HTTP协议相关
│   ├── net/            # 网络通信相关
│   └── proxy/          # 代理逻辑实现
├── CMakeLists.txt      # CMake构建配置
├── LICENSE             # 许可证文件
└── README.md           # 本文档
```

## 配置选项

可以通过命令行参数或配置文件设置以下选项：

- 端口号
- SSL/TLS证书路径
- 日志级别
- 并发连接数

详细配置请参考文档中的"配置"部分。

## 许可证

本项目采用MIT许可证。详见[LICENSE](LICENSE)文件。

## 作者

Liufeifan Liu Extraordinary

## 贡献

欢迎提交问题报告和合并请求！
