


- [为什么Redis集群有16384个槽](https://www.cnblogs.com/rjzheng/p/11430592.html)
出于带宽考虑


The reason is:
* Normal heartbeat packets carry the full configuration of a node, that can be replaced in an idempotent way with the old in order to update an old config. This means they contain the slots configuration for a node, in raw form, that uses 2k of space with16k slots, but would use a prohibitive 8k of space using 65k slots.


* At the same time it is unlikely that Redis Cluster would scale to more than 1000 mater nodes because of other design tradeoffs.

So 16k was in the right range to ensure enough slots per master with a max of 1000 maters, but a small enough number to propagate the slot configuration as a raw bitmap easily. Note that in small clusters the bitmap would be hard to compress because when N is small the bitmap would have slots/N bits set that is a large percentage of bits set.


node直接的心跳包中包含了node的slots配置信息，16k slots 使用 2k空间， 65kslots 使用8k空间

最多100 个 master的时候16k slots 足够使用了，大小也是合适传播



[【原创】分布式之redis复习精讲](https://www.cnblogs.com/rjzheng/p/9096228.html)

2、使用redis有什么缺点
分析:大家用redis这么久，这个问题是必须要了解的，基本上使用redis都会碰到一些问题，常见的也就几个。
回答:主要是四个问题
(一)缓存和数据库双写一致性问题
(二)缓存雪崩问题
(三)缓存击穿问题
(四)缓存的并发竞争问题
这四个问题，我个人是觉得在项目中，比较常遇见的，具体解决方案，后文给出。


3、单线程的redis为什么这么快
回答:主要是以下三点
(一)纯内存操作
(二)单线程操作，避免了频繁的上下文切换
(三)采用了非阻塞I/O多路复用机制


4、redis的数据类型，以及每种数据类型的使用场景

5、redis的过期策略以及内存淘汰机制

过期策略
	定期随机抽查，redis默认每个100ms检查
	惰性删除，在使用的时候，检查时否过期
淘汰机制
	1）noeviction：拒绝写入新数据
	2）allkeys-lru：在键空间中，移除最近最少使用的key
	3）allkeys-random：在键空间中，随机移除某个key。
	4）volatile-lru：在设置了过期时间的键空间中，移除最近最少使用的key。
	5）volatile-random：在设置了过期时间的键空间中，随机移除某个key
	6）volatile-ttl：在设置了过期时间的键空间中，有更早过期时间的key优先移除


6、redis和数据库双写一致性问题


7、如何应对缓存穿透和缓存雪崩问题

缓存穿透，即黑客故意去请求缓存中不存在的数据，导致所有的请求都怼到数据库上，从而数据库连接异常。
解决方案:
(一)利用互斥锁，缓存失效的时候，先去获得锁，得到锁了，再去请求数据库。没得到锁，则休眠一段时间重试
(二)采用异步更新策略，无论key是否取到值，都直接返回。value值中维护一个缓存失效时间，缓存如果过期，异步起一个线程去读数据库，更新缓存。需要做缓存预热(项目启动前，先加载缓存)操作。
(三)提供一个能迅速判断请求是否有效的拦截机制，比如，利用布隆过滤器，内部维护一系列合法有效的key。迅速判断出，请求所携带的Key是否合法有效。如果不合法，则直接返回。

缓存雪崩，即缓存同一时间大面积的失效，这个时候又来了一波请求，结果请求都怼到数据库上，从而导致数据库连接异常。
解决方案:
(一)给缓存的失效时间，加上一个随机值，避免集体失效。
(二)使用互斥锁，但是该方案吞吐量明显下降了。
(三)双缓存。我们有两个缓存，缓存A和缓存B。缓存A的失效时间为20分钟，缓存B不设失效时间。




map的现实
两种实现方法
开放寻址法：只有数组，冲突后，探测数组中的下一个索引的数据
拉链法：数组加上链表实现，冲突后，连接到连标上
搜索树法一般采用自平衡搜索树，包括：AVL 树，红黑树




char* 与 sds的区别
struct sdshdr {

    // 记录 buf 数组中已使用字节的数量
    // 等于 SDS 所保存字符串的长度
    int len;

    // 记录 buf 数组中未使用字节的数量
    int free;

    // 字节数组，用于保存字符串
    char buf[];

};

free 属性的值为 0 ， 表示这个 SDS 没有分配任何未使用空间。
len 属性的值为 5 ， 表示这个 SDS 保存了一个五字节长的字符串。
buf 属性是一个 char 类型的数组， 数组的前五个字节分别保存了 'R' 、 'e' 、 'd' 、 'i' 、 's' 五个字符， 而最后一个字节则保存了空字符 '\0' 。




2.2.1 常数复杂度获取字符串长度
2.2.2 杜绝缓冲区溢出
2.2.3 减少修改字符串时带来的内存重分配次数
1.空间预分配
2.惰性空间释放
2.2.4 二进制安全





调优：
<https://www.jianshu.com/p/44712ff0528d>
<https://juejin.im/post/5e7724be51882549315fd74b>
<https://zhuanlan.zhihu.com/p/61053939>
<https://yq.aliyun.com/articles/680237>
<https://zhuanlan.zhihu.com/p/41228196>
<https://www.bangbangde.com/post/gossip.html>
<https://juejin.im/post/5dd65d676fb9a05a9a22ac6f>


