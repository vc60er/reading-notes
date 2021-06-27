# 分布式id服务


## 全局唯一

## 全局唯一，趋势有序
趋势有序

## 全局唯一，局部有序

局部有序是相对于全局有序来说的。局部有序是指，在某个维度内有序，比如，在用户纬度内有序，即，分配给某个用户的id可以保证有序

### 微信msgid

**实现原理**

如果要保证有序，那么id必须**同时**由一个机器来来产生。然而，这种情况下高可用的方案就得通过主从的方式来实现。


**数据结构**

分布式存储记录:

|uid|max_seq|
|---|------|

内存中记录:

|uid|max_seq|cur_seq|
|---|------|-------|

**实现过程**

1. 分配服务从持久化存储中分配到一个id段：
    - 读取存储中的max_seq，赋值给内存中的: cur_seq mem.seq_seq = store.max_seq;
    - 将存储中的max_seq增加一个step，然后赋值给内存中的max_seq: store.max_seq+=step; mem.max_seq=store.max_seq;
2. 分配服务再将id段中的id逐个分配：mem.cur_seq++
3. 用完id段之后，再回到第1步: mem.cur_seq==mem.max_seq


**高可用**

由于一个uid的seq只能同时被一个服务分配，那么，seq的请求是如何准确到达后台服务的呢？又是如何实现高可用的呢？


通过配置来控制分配服务负责哪些uid，并且在分配服务出现故障的时候，将他负责的uid转给其他的分配服务。分配服务加载uid和卸载uid的过程如下：

1. 分配服务需要服务uid，被写入到分布式存储中 
2. 分配服务启动时，读取其需要提供服务的uid
3. 分配服务运行过程中，定期读取配置：
    - 如果读取失败，则需要在n秒内停止服务；
    - 如何读取成功，而且配置比较上次有变化，需要立即卸载需要卸载的uid，加载新的uid，等待n秒之后给新的uid提供服务


uid对应的分配服务，可能发生变动，比如，主从切块，认为调整负载。那么这些情况下，seq的请求是如何准确到达后台服务的呢？

概括来说，就是使用类似http 302，redis moved，这种类似的方式，当请求的uid不是当前的分配服务负责时，分配服务会返回正确的uid路由表，客户端收到后在缓存下来。

引述一段原文的详细描述：

    这里通过在 Client 端内存缓存路由表以及路由版本号来解决，请求步骤如下：
    1）Client 根据本地共享内存缓存的路由表，选择对应的AllocSvr；如果路由表不存在，随机选择一台AllocSvr；
    2）对选中的 AllocSvr 发起请求，请求带上本地路由表的版本号；
    3）AllocSvr 收到请求，除了处理 sequence 逻辑外，判断 Client 带上版本号是否最新，如果是旧版则在响应包中附上最新的路由表；
    4）Client收到响应包，除了处理 sequence 逻辑外，判断响应包是否带有新路由表。如果有，更新本地路由表，并决策是否返回第1步重试。



**问题**

1. uid在切换分配服务对的时候有n秒服务不可用，怎么办？

    由于微信后台逻辑层存在重试机制及异步重试队列，小段时间的不可服务是用户无感知的，而且出现租约失效、切换是小概率事件，整体上是可以接受的

2. 等待n秒的原因是什么？

    等待n秒的意义，在于保证uid的老服务已经卸载了他，即使，他的老服务服务读取到最新的配置，老服务也会因为读取不到配置而终止服务。

3. 仲裁服务是如何认定一个分配服务不可用的？
    todo



**优化改进**

为了进一步优化存储的io的次数，可以做以下改进。多个uid共用一个max_seq的情况，多个uid中的任何一个用完id段之后，更新一次max_seq，其他的uid也随之使用这个最新的max_seq，不用再去单独更新，省去不少io的次数。


分布式存储记录:

|uid|shared_max_seq|
|---|------|

内存中记录:

|uid|shared_max_seq|cur_seq|
|---|------|-------|


**参考资料**

- <http://www.52im.net/thread-1998-1-1.html>
- <http://www.52im.net/thread-1999-1-1.html>

