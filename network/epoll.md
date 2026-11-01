`epoll` 是 Linux 提供的高性能 I/O 多路复用机制，用于让一个线程同时监视大量文件描述符（尤其是 socket），并只处理真正发生 I/O 事件的连接。它是 `select`/`poll` 的增强版，特别适合“连接很多、活跃连接较少”的高并发服务器场景。[[man.archlinux](https://man.archlinux.org/man/epoll.7.zh_CN)][[zh.wikipedia](https://zh.wikipedia.org/wiki/Epoll)]

## 解决什么问题

传统 `select`/`poll` 每次等待事件时，通常需要把整个描述符集合从用户态传给内核，并在内核中遍历所有被监视的描述符；当连接数达到数万甚至更多时，开销会随连接数线性增长。[[zh.wikipedia](https://zh.wikipedia.org/wiki/Epoll)][[developer.aliyun](https://developer.aliyun.com/article/1633871)]

`epoll` 把“注册关注哪些 fd”和“等待哪些 fd 就绪”分开：fd 集合长期保存在内核的 epoll 实例中，事件发生时由内核回调将其放入就绪列表；`epoll_wait` 只返回就绪的 fd，不需要每次遍历全部连接。[[man.archlinux](https://man.archlinux.org/man/epoll.7.zh_CN)][[cloud.tencent](https://cloud.tencent.com/developer/article/2541541)]

## 三个核心 API

| API               | 作用                                             |
| ----------------- | ------------------------------------------------ |
| `epoll_create1()` | 创建一个 epoll 实例，返回一个 epoll 文件描述符   |
| `epoll_ctl()`     | 向实例中添加、修改或删除被监视的 fd 及其关注事件 |
| `epoll_wait()`    | 阻塞或限时等待，返回已就绪的事件列表             |

epoll 实例在内核中维护两个关键集合：**兴趣列表**保存用户注册的 fd 及关注事件；**就绪列表**保存当前可进行 I/O 的 fd。内核根据 fd 上的 I/O 活动动态填充就绪列表。[[man.archlinux](https://man.archlinux.org/man/epoll.7.zh_CN)]

```
int epfd = epoll_create1(0);

struct epoll_event ev;
ev.events = EPOLLIN;          // 关注可读事件
ev.data.fd = client_fd;
epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev);

struct epoll_event events[1024];
int n = epoll_wait(epfd, events, 1024, -1);

for (int i = 0; i < n; ++i) {
    int fd = events[i].data.fd;
    if (events[i].events & EPOLLIN) {
        // fd 可读，执行 read()
    }
}
```

## 内部工作原理

1. **注册阶段**：`epoll_ctl(EPOLL_CTL_ADD)` 将 fd 与关注事件封装成内核中的 `epitem`，插入 epoll 实例的红黑树，便于快速查找、修改和删除。[[zh.wikipedia](https://zh.wikipedia.org/wiki/Epoll)][[developer.aliyun](https://developer.aliyun.com/article/1589772)]
2. **事件回调**：当 socket 收到数据、发送缓冲区有空间、连接关闭等事件发生时，设备或协议栈通过注册的回调把对应 `epitem` 加入就绪链表。[[zh.wikipedia](https://zh.wikipedia.org/wiki/Epoll)][[developer.aliyun](https://developer.aliyun.com/article/1589772)]
3. **等待阶段**：`epoll_wait` 检查就绪链表；若有就绪事件，则把事件复制到用户传入的 `events` 数组并返回；否则让调用线程睡眠，直到事件到来或超时。[[man.archlinux](https://man.archlinux.org/man/epoll.7.zh_CN)][[cloud.tencent](https://cloud.tencent.com/developer/article/2541541)]
4. **处理阶段**：应用程序只遍历返回的就绪事件，对对应 fd 执行非阻塞 `read()`、`write()`、`accept()` 等操作。[[cloud.tencent](https://cloud.tencent.com/developer/article/2541541)][[developer.aliyun](https://developer.aliyun.com/article/1369550)]

因此，epoll 的性能优势主要来自：fd 集合只注册一次、避免重复拷贝、事件到来时通过回调入队，以及 `epoll_wait` 只处理活跃连接。[[cloud.tencent](https://cloud.tencent.com/developer/article/2541541)][[developer.aliyun](https://developer.aliyun.com/article/1604858)]

## LT 与 ET 模式

epoll 支持两种触发模式，通过 `epoll_ctl` 注册事件时设置：

| 模式                                | 触发条件                                                     | 特点                                                    | 典型用法                       |
| ----------------------------------- | ------------------------------------------------------------ | ------------------------------------------------------- | ------------------------------ |
| **LT（Level-Triggered，水平触发）** | 只要条件仍满足就持续通知，例如接收缓冲区仍有数据则每次 `epoll_wait` 都报 `EPOLLIN` | 编程简单，不容易漏事件                                  | 默认模式，适合大多数服务       |
| **ET（Edge-Triggered，边缘触发）**  | 只在状态发生变化时通知一次，例如从“无数据”变为“有数据”       | 更高效，但必须一次性把数据读/写完，否则可能不再收到通知 | 高性能服务器，常配合非阻塞 I/O |

例如在 ET 模式下收到 `EPOLLIN` 后，通常要循环 `read()` 直到返回 `EAGAIN` 或 `EWOULDBLOCK`，表示缓冲区暂时没有更多数据；否则剩余数据可能不会被再次通知。[[man.archlinux](https://man.archlinux.org/man/epoll.7.zh_CN)][[developer.aliyun](https://developer.aliyun.com/article/1369550)][[developer.cloud.tencent](https://developer.cloud.tencent.com/article/1488129?policyId)]

## 常用事件

- `EPOLLIN`：可读，包括新连接到达、对端发送数据、连接关闭等。
- `EPOLLOUT`：可写，通常在发送缓冲区曾满、之后又有空间时关注。
- `EPOLLERR`：发生错误。
- `EPOLLHUP`：对端挂断。
- `EPOLLRDHUP`：对端关闭写方向或关闭连接。
- `EPOLLET`：启用边缘触发模式。
- `EPOLLONESHOT`：事件通知一次后自动失效，需重新通过 `epoll_ctl` 重新注册，适合多线程避免同一 fd 被并发处理。

## 与 select、poll 对比

| 维度        | select                                | poll                     | epoll                                |
| ----------- | ------------------------------------- | ------------------------ | ------------------------------------ |
| fd 数量限制 | 通常受 `FD_SETSIZE` 限制，常见为 1024 | 无硬性固定上限           | 无 select 式固定上限，受系统资源限制 |
| 每次调用    | 每次都要传入整个 fd 集合              | 每次都要传入整个 fd 数组 | fd 集合保存在内核，注册一次即可      |
| 获取就绪 fd | 遍历全部 fd                           | 遍历全部 fd              | 只返回就绪 fd                        |
| 事件通知    | 水平触发                              | 水平触发                 | 支持水平触发和边缘触发               |
| 适用场景    | fd 少、跨平台需求                     | fd 较多但活跃度不确定    | Linux 高并发、大量长连接             |

epoll 的复杂度优势在“大量连接但只有少量活跃”时最明显；如果几乎所有连接都持续活跃，它相对于 `poll` 的优势会缩小。[[developer.aliyun](https://developer.aliyun.com/article/1633871)][[cloud.tencent](https://cloud.tencent.com/developer/article/2541541)][[developer.aliyun](https://developer.aliyun.com/article/1369550)]

## 典型服务器流程

1. 创建监听 socket，设置为非阻塞。
2. 创建 epoll 实例。
3. 将监听 socket 注册到 epoll，关注 `EPOLLIN`。
4. 调用 `epoll_wait` 等待事件。
5. 若监听 socket 可读，调用 `accept()` 获取新连接，并将其注册到 epoll。
6. 若客户端 socket 可读，读取数据并处理；若需要回复且当前不可写，注册 `EPOLLOUT`，等可写后再发送。
7. 连接关闭或出错时，关闭 fd 并调用 `epoll_ctl(EPOLL_CTL_DEL)` 移除。

## 实践注意事项

- **配合非阻塞 I/O**：尤其在 ET 模式下，阻塞的 `read()`/`write()` 可能让整个事件循环卡住。
- **ET 必须排空缓冲区**：读要读到 `EAGAIN`，写要写到 `EAGAIN` 或数据发送完成。
- **处理 `EINTR`**：`epoll_wait` 可能被信号中断，应重试。
- **及时处理错误和挂断**：`EPOLLERR`、`EPOLLHUP`、`EPOLLRDHUP` 都应触发关闭和资源回收。
- **不要盲目使用 ET**：LT 更安全、更易维护；ET 的收益通常只在极高性能、连接量大且活跃比例低的场景才显著。
- **epoll 不是异步 I/O**：它通知“fd 已就绪”，实际读写仍由应用程序完成；真正的异步 I/O 则由内核完成读写后通知结果。

简言之，epoll 的核心思想是：**把 fd 注册留在内核，用回调维护就绪队列，让应用只处理真正活跃的连接**。这使它成为 Nginx、Redis、Netty（Linux 下通过 epoll transport）等高性能网络服务的重要基础。[[cloud.tencent](https://cloud.tencent.com/developer/article/2541541)][[developer.aliyun](https://developer.aliyun.com/article/1369550)]