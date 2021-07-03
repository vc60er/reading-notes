# 分布式id服务



<!-- MarkdownTOC autolink=true -->

- [全局唯一](#%E5%85%A8%E5%B1%80%E5%94%AF%E4%B8%80)
    - [UUID](#uuid)
    - [SnowFlake](#snowflake)
- [全局唯一，趋势有序](#%E5%85%A8%E5%B1%80%E5%94%AF%E4%B8%80%EF%BC%8C%E8%B6%8B%E5%8A%BF%E6%9C%89%E5%BA%8F)
    - [mysql自增id](#mysql%E8%87%AA%E5%A2%9Eid)
    - [美团Leaf](#%E7%BE%8E%E5%9B%A2leaf)
    - [滴滴Tinyid](#%E6%BB%B4%E6%BB%B4tinyid)
    - [百度UidGenerator](#%E7%99%BE%E5%BA%A6uidgenerator)
- [全局唯一，局部有序](#%E5%85%A8%E5%B1%80%E5%94%AF%E4%B8%80%EF%BC%8C%E5%B1%80%E9%83%A8%E6%9C%89%E5%BA%8F)
    - [微信msgid](#%E5%BE%AE%E4%BF%A1msgid)
    - [美团Leaf-snowflake](#%E7%BE%8E%E5%9B%A2leaf-snowflake)

<!-- /MarkdownTOC -->


## 全局唯一
### UUID

UUID（Universally Unique Identifier）的标准型式包含32个16进制数字，以连字号分为五段，形式为8-4-4-4-12的36个字符，示例：550e8400-e29b-41d4-a716-446655440000，到目前为止业界一共有5种方式生成UUID，详情见IETF发布的UUID规范：[《A Universally Unique IDentifier (UUID) URN Namespace》](https://www.ietf.org/rfc/rfc4122.txt)。

todo


**缺点**

- 不易于存储：UUID太长，16字节128位，通常以36长度的字符串表示，很多场景不适用；
- 信息不安全：基于MAC地址生成UUID的算法可能会造成MAC地址泄露，这个漏洞曾被用于寻找梅丽莎病毒的制作者位置。

### SnowFlake

SnowFlake 算法，是 Twitter 开源的分布式 ID 生成算法。其核心思想就是：使用一个 64 bit 的 long 型的数字作为全局唯一 ID。


**实现方式**

这 64 个 bit 中，其中 1 个 bit 是不用的，然后用其中的 41 bit 作为毫秒数，用 10 bit 作为工作机器 ID，12 bit 作为序列号。

SnowFlake的ID构成：
![image](snowflake.png)

① 1 bit：是不用的，为啥呢？

因为二进制里第一个 bit 为如果是 1，那么都是负数，但是我们生成的 ID 都是正数，所以第一个 bit 统一都是 0。

② 41 bit：表示的是时间戳，单位是毫秒。

41 bit 可以表示的数字多达 2^41 - 1，也就是可以标识 2 ^ 41 - 1 个毫秒值，换算成年就是表示 69 年的时间。

③ 10 bit：记录工作机器 ID，代表的是这个服务最多可以部署在 2^10 台机器上，也就是 1024 台机器。

但是 10 bit 里 5 个 bit 代表机房 id，5 个 bit 代表机器 ID。意思就是最多代表 2 ^ 5 个机房（32 个机房），每个机房里可以代表 2 ^ 5 个机器（32 台机器）。

④12 bit：这个是用来记录同一个毫秒内产生的不同 ID。

12 bit 可以代表的最大正整数是 2 ^ 12 - 1 = 4096，也就是说可以用这个 12 bit 代表的数字来区分同一个毫秒内的 4096 个不同的 ID。理论上snowflake方案的QPS约为409.6w/s，这种分配方式可以保证在任何一个IDC的任何一台机器在任意毫秒内生成的ID都是不同的。


**缺点**

强依赖机器时钟，如果机器上时钟回拨，会导致发号重复或者服务会处于不可用状态。




## 全局唯一，趋势有序

### mysql自增id

**原理**

利用给字段设置auto_increment_increment和auto_increment_offset来保证ID自增，每次业务使用下列SQL读写MySQL得到ID号。

```
# 创建表
CREATE TABLE `Tickets64` (
  `id` bigint(20) unsigned NOT NULL auto_increment,
  `stub` char(1) NOT NULL default '',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `stub` (`stub`)
) ENGINE=InnoDB


# 获取新id
begin;
REPLACE INTO Tickets64 (stub) VALUES ('a');
SELECT LAST_INSERT_ID();
commit;

```

auto_increment_increment： 自增id增加的步长
auto_increment_offset：自增id的初始化值

REPLACE 的工作方式与 INSERT 完全相同，但如果表中的旧行与 PRIMARY KEY 或 UNIQUE 索引的新行具有相同的值，则在插入新行之前删除旧行。


**高可用**

可以部署多台mysql，每个服务以不同的初始值和相同的步长产生id

下面是部署两个mysql的例子：

```
TicketServer1:
auto-increment-increment = 2
auto-increment-offset = 1

TicketServer2:
auto-increment-increment = 2
auto-increment-offset = 2
```

**缺点**

- 系统水平扩展比较困难
- 数据库压力会成为瓶颈

**参考文档**

- <https://code.flickr.net/2010/02/08/ticket-servers-distributed-unique-primary-keys-on-the-cheap/>




### 美团Leaf


**原理**

Leaf服务预先从db中获取一个号段，然后在内存中将此号段进行分配，Leaf可以部署多个，多个Lefa先后获区了不同的号段，所以，最总分配出的结果不是严格有序，而是趋势有序的。

数据库表设计如下：

```
+-------------+--------------+------+-----+-------------------+-----------------------------+
| Field       | Type         | Null | Key | Default           | Extra                       |
+-------------+--------------+------+-----+-------------------+-----------------------------+
| biz_tag     | varchar(128) | NO   | PRI |                   |                             |
| max_id      | bigint(20)   | NO   |     | 1                 |                             |
| step        | int(11)      | NO   |     | NULL              |                             |
| desc        | varchar(256) | YES  |     | NULL              |                             |
| update_time | timestamp    | NO   |     | CURRENT_TIMESTAMP | on update CURRENT_TIMESTAMP |
+-------------+--------------+------+-----+-------------------+-----------------------------+
```


重要字段说明：

    biz_tag：用来区分业务；
    max_id：表示该biz_tag目前所被分配的ID号段的最大值；
    step：表示每次分配的号段长度。


Leaf获取号段的过程：

```
Begin
UPDATE table SET max_id=max_id+step WHERE biz_tag=xxx
SELECT tag, max_id, step FROM table WHERE biz_tag=xxx
Commit
```

**优化**

预获取号段的逻辑，可以在当前id快要用完的时候，提前获取。

**高可用**

依赖于db的可用性


### 滴滴Tinyid
### 百度UidGenerator




## 全局唯一，局部有序

局部有序是相对于全局有序来说的。局部有序是指，在某个维度内有序，比如，在用户纬度内有序，即，分配给某个用户的id可以保证有序

### 微信msgid

同一uid内有序

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



### 美团Leaf-snowflake

同一机器内有序

**原理**

严格来说，Leaf-snowflake方案是Twittersnowflake改进版，它完全沿用snowflake方案的bit位设计，即是“1+41+10+12”的方式组装ID号。

Leaf-snowflake在workid的配置和系统时间回退问题上做了一些优化

1. 通过zk的顺序持久节点生workerid，减少了workerid的维护成本
2. 启动前检查系统的时钟是否有回退，检查系统的时钟是否与其他的服务的时钟有较大差距，如果有，则终止服务启动。


**启动过程**

- 启动时读取zk的顺序节点中记录workerid和worker的系统时间
- 如果没有workerid，则以当前服务的ip:port做为key，写入zk顺序节点，得到workerid
- 检查服务当前时间是否小于worker时间，如果小于则，终止服务
- 在顺序节点中写入当前服务的系统时间
- 通过rpc访问其他服务获取其他服务的系统时间
- 检查 abs(系统时间-sum(time)/nodeSize) >= 阈值，如果大于，则终止服务
- 创建zk临时节点，并且每隔一段时间(3s)上报自身系统时间，写入临时节点


**参考资料**

- <http://www.52im.net/thread-2751-1-1.html>






