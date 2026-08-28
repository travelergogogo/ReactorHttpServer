# 04 - 非阻塞 ClientFd 与 recv 循环读取

## 一、本阶段目标

上一阶段已经完成了 `TcpConnection` 的生命周期管理：

```text
accept
    ↓
创建 TcpConnection
    ↓
connections_ 保存
    ↓
epoll 监听
    ↓
连接断开
    ↓
epoll DEL
    ↓
connections_ erase
    ↓
TcpConnection 析构
    ↓
close(fd)
```

这一阶段主要解决另外一个非常重要的问题：

> 一个客户端不能因为阻塞 IO 卡住整个 EventLoop。

主要完成：

1. 将 `accept()` 返回的 `clientfd` 设置为非阻塞。
2. 将原来只调用一次的 `recv()` 改成循环读取。
3. 一直读取到 `EAGAIN / EWOULDBLOCK`。
4. 正确区分 `EAGAIN`、`EINTR`、`recv == 0`。
5. 理解 `peerClosed` 的作用。
6. 理解 `handleRead()` 返回值 `alive` 的作用。
7. 使用 `nc + curl` 验证一个空闲客户端不会阻塞其他客户端。

最终形成：

```text
clientfd nonblocking
        +
循环 recv
        +
一直读取到 EAGAIN
```

---

# 二、为什么 listenfd 非阻塞还不够

之前已经有：

```cpp
socket_.setNonBlocking();
```

其实现大致为：

```cpp
void Socket::setNonBlocking()
{
    int flags = fcntl(listenfd_, F_GETFL, 0);

    flags |= O_NONBLOCK;

    fcntl(listenfd_, F_SETFL, flags);
}
```

这里设置的是：

```text
listenfd_
```

它解决的是：

```text
当前没有客户端等待连接
        ↓
accept()
        ↓
不会一直阻塞
```

但是：

```cpp
int clientfd = accept(...);
```

得到的是一个新的文件描述符。

不能因为：

```text
listenfd 是 nonblocking
```

就认为：

```text
clientfd 也是 nonblocking
```

所以可能出现：

```text
listenfd
    ↓
nonblocking

clientfd
    ↓
blocking
```

这会产生一个严重问题。

假设以后 `handleRead()` 写成：

```cpp
while(true)
{
    recv(...);
}
```

第一次 `recv()`：

```text
收到 78 bytes
```

继续第二次 `recv()`：

```text
当前已经没有数据
```

如果 `clientfd` 是阻塞的，那么：

```text
recv()
    ↓
一直等待客户端继续发送
    ↓
当前线程被阻塞
    ↓
EventLoop 无法继续运行
    ↓
其他客户端无法被处理
```

因此：

```text
listenfd nonblocking
```

主要保证：

```text
accept 不阻塞
```

而：

```text
clientfd nonblocking
```

主要保证：

```text
recv / send 不因为某一个客户端长期阻塞 EventLoop
```

---

# 三、如何将 clientfd 设置为 nonblocking

`accept()` 成功：

```cpp
int clientfd = accept(
    listenfd_,
    reinterpret_cast<sockaddr*>(&clientAddr),
    &len
);
```

首先判断：

```cpp
if(clientfd < 0)
{
    return -1;
}
```

之后获取原来的 flags：

```cpp
int flags = fcntl(clientfd, F_GETFL, 0);
```

添加：

```cpp
O_NONBLOCK
```

```cpp
flags |= O_NONBLOCK;
```

最后设置回去：

```cpp
fcntl(clientfd, F_SETFL, flags);
```

完整过程：

```text
F_GETFL
    ↓
获取 clientfd 原来的 flags
    ↓
flags |= O_NONBLOCK
    ↓
F_SETFL
    ↓
clientfd 真正成为 nonblocking
```

---

# 四、F_GETFL 和 F_SETFL 的区别

## F_GETFL

```cpp
fcntl(fd, F_GETFL, 0);
```

作用：

> 获取当前 fd 的状态标志。

例如：

```cpp
int flags = fcntl(clientfd, F_GETFL, 0);
```

---

## F_SETFL

```cpp
fcntl(fd, F_SETFL, flags);
```

作用：

> 把修改后的状态标志重新设置到 fd。

例如：

```cpp
flags |= O_NONBLOCK;

fcntl(clientfd, F_SETFL, flags);
```

这里一个容易犯的错误是：

```cpp
fcntl(clientfd, F_GETFL, flags);
```

第二次仍然写成 `F_GETFL`。

这是错误的。

正确过程是：

```text
GET
↓
修改
↓
SET
```

也就是：

```cpp
int flags = fcntl(clientfd, F_GETFL, 0);

flags |= O_NONBLOCK;

fcntl(clientfd, F_SETFL, flags);
```

---

# 五、为什么 flags |= O_NONBLOCK 还不够

执行：

```cpp
flags |= O_NONBLOCK;
```

只是修改了程序中的：

```cpp
int flags;
```

这个普通整数变量。

此时：

```text
程序里的 flags
    ↓
已经包含 O_NONBLOCK
```

但：

```text
内核里的 clientfd
    ↓
还没有发生改变
```

所以还需要：

```cpp
fcntl(clientfd, F_SETFL, flags);
```

才能真正修改内核中这个文件描述符的状态。

因此：

```text
F_GETFL
→ 获取

flags |= O_NONBLOCK
→ 修改本地变量

F_SETFL
→ 真正设置回 fd
```

---

# 六、fcntl 设置失败为什么不能继续返回 clientfd

现在希望 `Socket::acceptClient()` 有一个明确的函数约定：

```text
clientfd >= 0
```

表示：

1. `accept()` 成功。
2. `clientfd` 已成功设置为 nonblocking。

如果：

```cpp
fcntl(clientfd, F_SETFL, flags)
```

失败以后，我们只是：

```cpp
perror(...);
```

但仍然：

```cpp
return clientfd;
```

那么 WebServer 会认为：

```text
这是一个正常的 nonblocking clientfd
```

但实际上它可能还是 blocking。

之后：

```text
handleRead
    ↓
循环 recv
    ↓
当前数据读完
    ↓
继续 recv
    ↓
阻塞
    ↓
EventLoop 被卡住
```

所以设置失败以后应该：

```cpp
close(clientfd);
return -1;
```

例如：

```cpp
int flags = fcntl(clientfd, F_GETFL, 0);

if(flags == -1)
{
    perror("fcntl(GET)");
    close(clientfd);
    return -1;
}

flags |= O_NONBLOCK;

if(fcntl(clientfd, F_SETFL, flags) == -1)
{
    perror("fcntl(SET)");
    close(clientfd);
    return -1;
}

return clientfd;
```

---

# 七、为什么这里需要 close(clientfd)

在这一阶段：

```text
accept()
    ↓
获得 clientfd
    ↓
设置 nonblocking
```

此时：

```text
TcpConnection 还没有创建
```

也就是说，fd 还没有交给 `TcpConnection` 管理。

如果设置失败：

```text
accept 成功
    ↓
得到 fd=5
    ↓
fcntl 失败
    ↓
直接 return -1
```

如果不执行：

```cpp
close(clientfd);
```

那么 fd=5 就没有任何对象继续管理，会发生：

```text
文件描述符泄漏
```

所以这里需要：

```text
配置成功
→ 把 fd 交给后面的 TcpConnection

配置失败
→ 当前函数负责 close
```

---

# 八、WebServer 为什么需要判断 clientfd < 0

之前可能直接写：

```cpp
int clientfd = socket_.acceptClient();

auto conn =
    std::make_shared<TcpConnection>(clientfd);

connections_[clientfd] = conn;

epoll_.addFd(clientfd, EPOLLIN);
```

但是：

```cpp
acceptClient()
```

现在可能返回：

```text
-1
```

所以必须：

```cpp
int clientfd = socket_.acceptClient();

if(clientfd < 0)
{
    continue;
}
```

否则可能产生：

```cpp
TcpConnection(-1);
```

甚至：

```cpp
epoll_.addFd(-1, EPOLLIN);
```

---

# 九、为什么这里使用 continue

当前代码大致位于：

```cpp
for(int i = 0; i < n; ++i)
{
    // 处理 epoll_wait 返回的事件
}
```

如果某个 `acceptClient()` 失败：

```cpp
if(clientfd < 0)
{
    continue;
}
```

表示：

```text
当前这个事件不继续处理
        ↓
继续处理 epoll_wait 返回的下一个事件
```

需要区分：

```text
continue
→ 跳过当前循环
→ 继续下一个事件

break
→ 整个 for 循环结束
→ 后面的事件也不处理

return
→ WebServer::start() 直接退出
→ 整个服务器停止
```

所以这里使用：

```cpp
continue;
```

---

# 十、为什么 recv 需要循环读取

当前临时 Buffer：

```cpp
char buf[4096];
```

一次最多读：

```text
4096 bytes
```

但是 socket 接收缓冲区中可能已经存在：

```text
10000 bytes
```

第一次：

```text
recv
↓
4096 bytes
```

还剩：

```text
5904 bytes
```

如果只调用一次 `recv()`：

```text
EPOLLIN
    ↓
只读4096
    ↓
剩余数据还留在内核
```

所以应该循环读取：

```text
recv
    ↓
n > 0
    ↓
append
    ↓
继续 recv
    ↓
n > 0
    ↓
append
    ↓
继续
    ↓
直到 EAGAIN
```

也就是说：

> 一次 EPOLLIN 到来以后，尽量把当前已经到达 socket 接收缓冲区的数据全部读取到用户态。

---

# 十一、recv 返回值的含义

这是本阶段非常重要的知识点。

---

## 1. n > 0

```cpp
ssize_t n = recv(...);
```

如果：

```cpp
n > 0
```

表示：

> 成功读取到了 n 个字节。

处理：

```cpp
inputBuffer_.append(buf, n);
continue;
```

为什么 `continue`？

因为 socket 接收缓冲区中可能还有更多数据。

---

## 2. n == 0

如果：

```cpp
recv(...) == 0
```

表示：

> 对端已经关闭发送方向，我们这一侧读到了 EOF。

它不是：

```text
EAGAIN
```

也不是：

```text
普通错误
```

所以：

```text
n == 0
```

以后不能再使用旧的 `errno` 判断当前状态。

---

## 3. EAGAIN / EWOULDBLOCK

对于 nonblocking socket：

```text
当前接收缓冲区没有数据
        ↓
recv()
        ↓
-1
        ↓
errno = EAGAIN / EWOULDBLOCK
```

它表示：

> 当前暂时没有更多数据可以读取。

它不代表：

```text
连接已经断开
```

所以不能：

```cpp
return false;
```

而应该：

```cpp
break;
```

退出当前 recv 循环。

流程：

```text
recv
↓
EAGAIN
↓
当前已经暂时读空
↓
退出 recv 循环
↓
处理当前数据
↓
handleRead 返回
↓
EventLoop 继续处理其他 fd
↓
以后这个 fd 再次可读
↓
epoll 再次通知
```

可以记成：

> EAGAIN：现在没有数据，不代表以后没有数据。

---

## 4. EINTR

如果：

```text
recv
↓
被 signal 打断
↓
返回 -1
↓
errno = EINTR
```

表示：

> 这次系统调用没有正常执行完成。

它并没有告诉我们：

```text
socket 已经读空
```

所以应该：

```cpp
continue;
```

重新调用 `recv()`。

---

# 十二、EAGAIN 和 EINTR 的区别

这两个很容易混淆。

```text
EAGAIN
```

表示：

> 当前没有数据了。

所以：

```cpp
break;
```

---

```text
EINTR
```

表示：

> 这一次 recv 被信号打断了，没有正常完成。

所以：

```cpp
continue;
```

重新尝试。

可以记成：

```text
EAGAIN
→ 没数据了
→ break

EINTR
→ 没读成
→ retry
→ continue
```

---

# 十三、当前 recv 循环结构

```cpp
bool peerClosed = false;

char buf[4096];

while(true)
{
    ssize_t n = recv(
        fd_,
        buf,
        sizeof(buf),
        0
    );

    if(n > 0)
    {
        inputBuffer_.append(buf, n);
        continue;
    }

    if(n == 0)
    {
        peerClosed = true;
        break;
    }

    if(errno == EINTR)
    {
        continue;
    }

    if(errno == EAGAIN ||
       errno == EWOULDBLOCK)
    {
        break;
    }

    perror("recv");
    return false;
}
```

---

# 十四、为什么这里使用 while(true)

一开始容易想到：

```cpp
while(!peerClosed)
```

但实际上：

```text
peerClosed
```

并不是退出 recv 循环的唯一条件。

例如：

```text
recv
↓
EAGAIN
```

此时：

```cpp
peerClosed == false;
```

客户端根本没有关闭。

但是当前已经：

```text
没有更多数据可以读取
```

所以我们仍然必须：

```cpp
break;
```

因此 recv 循环是否继续取决于：

```text
n > 0
n == 0
EINTR
EAGAIN
其他错误
```

而不仅仅取决于：

```text
peerClosed
```

所以：

```cpp
while(true)
```

配合：

```text
continue
break
return
```

更加符合这里的逻辑。

可以理解为：

> 我不知道到底要 recv 几次，每一次 recv 的结果决定下一步应该继续还是退出。

---

# 十五、peerClosed 到底是什么

这是本阶段最容易绕的地方之一。

定义：

```cpp
bool peerClosed = false;
```

它不是：

```text
HTTP 请求完整了吗？
```

也不是：

```text
这次有没有收到数据？
```

它只是一个：

> 状态记录变量。

它记录：

> 本轮 recv 循环有没有遇到 `recv() == 0`。

也就是：

```text
peerClosed == false
```

表示：

> 暂时没有发现对端关闭发送方向。

而：

```text
peerClosed == true
```

表示：

> 已经收到 EOF，对端以后不会继续向我们发送新的 TCP 字节。

可以把它理解成：

> 对端关闭状态的“备忘录”。

---

# 十六、为什么 recv == 0 不直接 return false

假设发生：

```text
第一次 recv
→ 78 bytes
→ append 到 inputBuffer_

第二次 recv
→ 0
```

此时：

```text
对端已经关闭发送方向
```

但是：

```text
之前收到的 78 bytes
```

还没有被 HTTP 层处理。

如果直接：

```cpp
if(n == 0)
{
    return false;
}
```

那么函数直接结束：

```text
78字节已经收到
    ↓
但是没有 parse
    ↓
没有生成 response
    ↓
没有 send
    ↓
直接告诉 WebServer 删除连接
```

这显然不合理。

所以改成：

```cpp
if(n == 0)
{
    peerClosed = true;
    break;
}
```

它表达的是：

> 我已经知道客户端不会继续发数据了，但是已经收到的数据还需要先处理。

因此：

```text
recv 收到数据
    ↓
append
    ↓
recv == 0
    ↓
peerClosed = true
    ↓
break
    ↓
退出 recv 循环
    ↓
处理 inputBuffer_
    ↓
处理结束以后
    ↓
再根据 peerClosed 决定连接是否关闭
```

---

# 十七、peerClosed 不能理解为 HTTP 请求已经完整

这一点必须特别注意。

```cpp
peerClosed == true
```

只能说明：

> 对端已经关闭 TCP 的发送方向。

它不能说明：

> HTTP 请求一定正确、完整。

例如对方发送：

```text
GET /index
```

然后直接关闭。

服务器可能：

```text
recv
→ "GET /index"

recv
→ 0
```

此时：

```cpp
peerClosed = true;
```

但这显然不是一个完整的 HTTP 请求。

所以：

```text
peerClosed
```

属于：

```text
TCP连接状态
```

而：

```text
HTTP请求是否完整
```

属于：

```text
HTTP协议解析状态
```

二者不能混淆。

---

# 十八、为什么最后 return !peerClosed

当前：

```cpp
TcpConnection::handleRead()
```

返回值约定是：

```text
true
→ 当前 TcpConnection 继续存在

false
→ 当前 TcpConnection 应该被清理
```

但是：

```cpp
peerClosed == true
```

表示：

> 对端已经关闭发送方向。

所以：

```cpp
return !peerClosed;
```

正好形成：

```text
peerClosed = false
        ↓
!peerClosed = true
        ↓
连接继续存在
```

以及：

```text
peerClosed = true
        ↓
!peerClosed = false
        ↓
WebServer 清理连接
```

---

# 十九、为什么 inputBuffer 为空时也判断 peerClosed

recv 循环结束以后可能出现：

```text
inputBuffer_ 是空的
```

如果：

```text
peerClosed == false
```

可能只是：

```text
EAGAIN
```

表示：

> 当前没数据，但是连接仍然正常。

应该：

```cpp
return true;
```

如果：

```text
peerClosed == true
```

表示：

```text
对端已经关闭
+
当前也没有任何数据需要处理
```

应该：

```cpp
return false;
```

所以可以：

```cpp
if(inputBuffer_.empty())
{
    return !peerClosed;
}
```

---

# 二十、alive 到底是什么

WebServer 当前：

```cpp
auto it = connections_.find(fd);

if(it != connections_.end())
{
    bool alive =
        it->second->handleRead();

    if(!alive)
    {
        epoll_.delFd(fd);
        connections_.erase(it);
    }
}
```

这里：

```cpp
alive
```

不是：

> 有没有读取到数据。

它表示：

> 这次可读事件全部处理结束以后，这个 TcpConnection 是否还应该继续存在。

例如：

```text
recv → 78 bytes
recv → EAGAIN
```

表示：

```text
暂时读完
但是连接没关闭
```

所以：

```text
alive = true
```

WebServer：

```text
不删除连接
```

以后 fd 再有数据：

```text
epoll 再次通知
```

---

如果：

```text
recv → 78 bytes
recv → 0
```

则：

```text
peerClosed = true
```

已有数据处理完成以后：

```cpp
return !peerClosed;
```

得到：

```text
false
```

于是：

```text
alive = false
```

WebServer 执行：

```cpp
epoll_.delFd(fd);

connections_.erase(it);
```

最后：

```text
shared_ptr 引用计数归零
    ↓
TcpConnection 析构
    ↓
close(fd)
```

---

# 二十一、handleRead 当前到底做了什么

目前的：

```cpp
handleRead()
```

实际上不仅仅负责：

```text
recv
```

它目前还做：

```text
recv
    ↓
inputBuffer_
    ↓
HttpRequest::parse
    ↓
HttpResponse
    ↓
send
```

所以当前阶段更准确的理解是：

> `handleRead()` 暂时是在“处理一次可读事件”。

而不仅仅是单纯执行读取。

这也是为什么：

```cpp
bool alive = handleRead();
```

表示：

> 整次可读事件处理完成以后，这个连接是否继续存在。

---

# 二十二、长期来看 handleRead 为什么应该拆分

当前：

```cpp
handleRead()
```

同时负责：

```text
网络读取
+
Buffer
+
HTTP解析
+
生成响应
+
网络发送
```

职责过多。

长期容易产生：

1. 函数越来越大。
2. 网络层和 HTTP 层严重耦合。
3. 修改 HTTP 逻辑可能影响网络 IO。
4. 修改发送逻辑也需要修改 `handleRead()`。
5. 不方便实现真正的非阻塞写。
6. 不方便更换其他协议。
7. 不方便测试和维护。

最终更合理的结构会逐渐变成：

```text
EPOLLIN
    ↓
TcpConnection::handleRead()
    ↓
recv
    ↓
inputBuffer_
    ↓
messageCallback
    ↓
HTTP Parser
    ↓
HttpResponse
    ↓
outputBuffer_
    ↓
EPOLLOUT
    ↓
TcpConnection::handleWrite()
```

也就是说：

```text
TcpConnection
→ 负责网络连接和字节收发

HTTP Parser
→ 负责 HTTP 协议

HttpResponse
→ 负责响应生成

handleWrite
→ 负责发送 outputBuffer
```

---

# 二十三、当前 alive 也是过渡设计

当前关闭连接：

```text
TcpConnection::handleRead()
    ↓
return false
    ↓
WebServer
    ↓
epoll DEL
    ↓
connections_ erase
```

未来真正 Reactor 化以后可能变成：

```text
EventLoop
    ↓
Channel
    ↓
readCallback
    ↓
TcpConnection::handleRead()
    ↓
发现连接关闭
    ↓
closeCallback
    ↓
TcpServer::removeConnection()
```

所以以后：

```cpp
bool handleRead();
```

可能会重新变成：

```cpp
void handleRead();
```

由 callback 通知连接管理者进行清理。

因此当前：

```cpp
bool alive = it->second->handleRead();
```

属于项目演进过程中的过渡实现。

---

# 二十四、EAGAIN 为什么不等于 HTTP 请求完整

假设完整 HTTP 请求：

```text
GET /index.html HTTP/1.1\r\n
Host: example.com\r\n
\r\n
```

TCP 第一次可能只收到：

```text
GET /index.ht
```

然后：

```text
recv
↓
EAGAIN
```

EAGAIN 只能说明：

> 当前 socket 接收缓冲区暂时没有更多 TCP 字节。

它不能说明：

> HTTP 请求已经完整。

客户端以后仍然可能发送：

```text
ml HTTP/1.1\r\n
Host: example.com\r\n
\r\n
```

因此必须严格区分：

```text
EAGAIN
→ TCP / 非阻塞 IO 层状态
```

而：

```text
HTTP 请求完整
→ HTTP 协议层状态
```

这也是后面需要：

```text
Buffer
+
增量 HTTP Parser
+
NeedMore
+
Complete
+
BadRequest
```

的原因。

---

# 二十五、双客户端测试

为了验证：

> 某一个客户端不能通过阻塞 recv 卡住整个 EventLoop。

首先启动服务器：

```bash
./build/HttpServer
```

然后客户端 A：

```bash
nc 127.0.0.1 8888
```

连接以后：

```text
什么都不发送
```

服务器：

```text
new client fd=5
```

保持这个连接不动。

再开启客户端 B：

```bash
curl -v http://127.0.0.1:8888/
```

服务器又：

```text
new client fd=6
```

并且成功处理：

```text
GET / HTTP/1.1
```

curl 得到：

```text
HTTP/1.1 200 OK
```

这证明：

```text
fd=5
已经连接
但一直不发送数据

        ↓

没有阻塞 EventLoop

        ↓

fd=6
仍然能够建立连接

        ↓

fd=6
仍然能够被正常处理
```

所以 clientfd nonblocking 已经真正发挥作用。

---

# 二十六、如果 clientfd 是 blocking，这个实验会怎么样

如果 `clientfd` 是 blocking，并且：

```cpp
handleRead()
```

中不断：

```cpp
recv()
```

那么某个客户端进入读取以后：

```text
recv
↓
读完当前数据
↓
继续 recv
↓
没有数据
↓
阻塞
```

EventLoop 会停在这个客户端上。

此时：

```text
另一个客户端即使已经连接
```

也无法及时被当前线程处理。

所以：

> Reactor 中最重要的原则之一，就是不能让一个客户端长期占住 EventLoop 线程。

---

# 二十七、测试中观察到 fd 复用

测试中出现：

```text
connection closed fd=5
```

之后新的客户端连接：

```text
new client fd=5
```

说明：

> Linux 会复用已经释放的文件描述符编号。

流程：

```text
旧客户端
fd=5
↓
close(5)
↓
系统回收数字5
↓
新客户端建立连接
↓
系统可能再次返回 fd=5
```

这也说明之前为什么必须避免：

```text
double close
```

因为如果旧代码在 fd 被复用以后又执行：

```cpp
close(5);
```

此时关闭的可能已经不是旧客户端，而是一个新的连接。

因此当前关闭流程坚持：

```text
epoll DEL
    ↓
connections_ erase
    ↓
shared_ptr 引用计数归零
    ↓
TcpConnection 析构
    ↓
close(fd)
```

让 fd 的关闭责任尽量保持唯一。

---

# 二十八、本阶段完整调用链

## 1. 新客户端连接

```text
listenfd EPOLLIN
    ↓
Socket::acceptClient()
    ↓
accept()
    ↓
得到 clientfd
    ↓
F_GETFL
    ↓
加入 O_NONBLOCK
    ↓
F_SETFL
    ↓
返回 clientfd
```

WebServer：

```text
clientfd >= 0
    ↓
创建 TcpConnection
    ↓
connections_[clientfd]
    ↓
epoll ADD
```

---

## 2. clientfd 可读

```text
epoll_wait
    ↓
clientfd EPOLLIN
    ↓
connections_.find(fd)
    ↓
找到 TcpConnection
    ↓
handleRead()
```

进入 recv 循环：

```text
recv
│
├── n > 0
│      ↓
│    append
│      ↓
│    continue
│
├── EINTR
│      ↓
│    continue
│
├── EAGAIN / EWOULDBLOCK
│      ↓
│    break
│
├── n == 0
│      ↓
│    peerClosed = true
│      ↓
│    break
│
└── 其他错误
       ↓
     return false
```

---

## 3. 处理已经收到的数据

```text
inputBuffer_
    ↓
HttpRequest::parse
    ↓
HttpResponse
    ↓
send
```

最后：

```cpp
return !peerClosed;
```

---

## 4. WebServer 根据 alive 判断连接状态

```cpp
bool alive =
    it->second->handleRead();

if(!alive)
{
    epoll_.delFd(fd);
    connections_.erase(it);
}
```

形成：

```text
alive == true
    ↓
连接继续保留

alive == false
    ↓
epoll DEL
    ↓
connections_ erase
    ↓
TcpConnection 析构
    ↓
close(fd)
```

---

# 二十九、本阶段仍然没有解决的问题

本阶段只解决：

```text
clientfd nonblocking
+
recv until EAGAIN
```

目前仍然存在下面的问题。

---

## 1. HTTP 半包

当前：

```cpp
std::string request =
    inputBuffer_.readAll();
```

会直接把 Buffer 清空。

如果只收到：

```text
GET /index.ht
```

就执行 `readAll()`，可能导致正常的半包请求被误判为错误。

以后需要：

```text
增量 HTTP Parser
```

能够区分：

```text
NeedMore
Complete
BadRequest
```

---

## 2. send 可能部分发送

当前：

```cpp
send(
    fd_,
    data.c_str(),
    data.size(),
    0
);
```

一次 `send()` 不能保证把所有数据全部发送出去。

以后需要：

```text
outputBuffer_
+
EPOLLOUT
+
handleWrite()
```

---

## 3. handleRead 职责过多

当前：

```text
read
+
HTTP parse
+
response
+
send
```

仍然耦合在一起。

以后需要进一步解耦。

---

## 4. 当前关闭逻辑还是过渡设计

目前：

```text
handleRead
↓
bool alive
↓
WebServer 删除
```

以后会演化为：

```text
TcpConnection
↓
closeCallback
↓
TcpServer
↓
removeConnection
```

---

# 三十、本阶段最容易混淆的几个点

## 1. listenfd 和 clientfd

```text
listenfd nonblocking
→ 防止 accept 阻塞

clientfd nonblocking
→ 防止 recv/send 阻塞
```

---

## 2. EAGAIN

```text
当前暂时没有数据
```

不等于：

```text
连接断开
```

所以：

```text
EAGAIN
→ break
→ 回 EventLoop
→ 以后继续处理
```

---

## 3. EINTR

```text
当前 recv 被信号打断
```

所以：

```text
EINTR
→ continue
→ retry
```

---

## 4. recv == 0

```text
对端关闭发送方向
→ EOF
```

但是：

```text
之前已经收到的数据仍然需要处理
```

所以不能简单地立刻 `return false`。

---

## 5. peerClosed

```text
peerClosed
```

表示：

> 是否已经发现对端关闭发送方向。

它不表示：

```text
HTTP请求已经完整
```

---

## 6. alive

```text
alive
```

表示：

> 当前可读事件全部处理完成以后，这个 TcpConnection 是否应该继续存在。

它不表示：

```text
本次有没有收到数据
```

---

## 7. EAGAIN 与 HTTP 完整性

```text
EAGAIN
→ IO层状态

HTTP请求完整
→ 协议层状态
```

二者不是一个概念。

---

## 8. while(true) 和 peerClosed

`peerClosed` 不是退出循环的唯一原因。

```text
EAGAIN
```

发生时：

```text
peerClosed == false
```

但同样应该退出当前 recv 循环。

因此：

```cpp
while(true)
```

配合：

```text
continue
break
return
```

更加合理。

---

# 三十一、本阶段核心理解

本阶段最重要的不是单纯学会：

```cpp
fcntl
```

或者：

```cpp
while(recv)
```

而是理解 Reactor 的一个核心思想：

> EventLoop 线程不能因为某一个客户端的阻塞 IO 长时间停下来。

因此：

```text
一个 EventLoop
        ↓
管理很多 fd
        ↓
每个 fd 的 IO 都尽量 nonblocking
        ↓
当前不能做
        ↓
立即返回
        ↓
继续处理其他 fd
        ↓
以后由 epoll 再次通知
```

这才是：

```text
nonblocking IO + epoll + Reactor
```

能够管理大量连接的重要基础。