# TcpConnection 生命周期管理

## 修改前的问题

TcpConnection 如果作为局部变量创建，只能存活一次事件处理过程，
无法保存一个 TCP 连接跨多次 IO 事件的状态。

## 本次修改

### 1. 增加连接表

WebServer 中增加：

std::unordered_map<int, std::shared_ptr<TcpConnection>> connections_;

用于建立：

clientfd -> TcpConnection

的映射。

### 2. 使用 shared_ptr 管理连接生命周期

accept 成功后创建 TcpConnection：

std::make_shared<TcpConnection>(clientfd);

并保存进 connections_。

### 3. handleRead 返回连接状态

true：连接继续存活
false：连接需要关闭

### 4. Epoll 增加 delFd

连接关闭时先从 epoll 中删除对应 fd。

### 5. TcpConnection 使用 RAII 管理 fd

TcpConnection 析构时执行 close(fd_)。

## 最终生命周期

accept
→ 创建 TcpConnection
→ 加入 connections_
→ epoll 监听
→ handleRead
→ 客户端断开
→ epoll DEL
→ connections_ erase
→ shared_ptr 引用计数归零
→ TcpConnection 析构
→ close(fd)

## 验证

使用：

curl http://127.0.0.1:8888/

服务器日志：

new client fd=5
recv size: 78
recv size: 0
connection closed fd=5