#!/bin/bash

# 生成自签名证书和私钥
openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
  -keyout server.key -out server.crt \
  -subj "/C=CN/ST=State/L=City/O=Organization/CN=localhost"

# 设置权限
chmod 600 server.key
chmod 644 server.crt

echo "自签名证书已生成"
echo "  - 私钥: server.key"
echo "  - 证书: server.crt"
echo ""
echo "注意: 这是自签名证书，浏览器会显示不安全警告。"
