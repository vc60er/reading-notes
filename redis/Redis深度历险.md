# Redis 深度历险：核心原理和应用实践


## 应用 1:千帆竞发 —— 分布式锁

```shell
> set lock:codehole random ex 5 nx 
OK 
... do something critical ... 
> if get lock:codehole == random then del lock:codehole else return 0; end
```



## 应用 2:缓兵之计 —— 延时队列

**异步消息队列**

使用rpush/lpush操作入队列， 使用 lpop 和 rpop 来出队列。

阻塞读 blpop/brpop 


**延时队列**

用途：用于处理一段时间之后进行的任务。

实现方式：

延时队列可以通过 Redis 的 zset(有序列表) 来实现。我们将消息序列化成一个字符串作 为 zset 的 value，这个消息的到期处理时间作为 score，然后用多个线程轮询 zset 获取到期 的任务进行处理

zrem 方法是多线程多进程争抢任务的关键，它的返回值决定了当前实例有没有抢到任务， 因为 loop 方法可能会被多个线程、多个进程调用，同一个任务可能会被多个进程线程抢到，通过 zrem 来决定唯一的属主。


```python
def delay(msg):
	msg.id = str(uuid.uuid4()) # 保证 value 值唯一 value = json.dumps(msg)
	retry_ts = time.time() + 5 # 5 秒后重试 redis.zadd("delay-queue", retry_ts, value)

def loop(): 
	while True:
		# 最多取 1 条
		values = redis.zrangebyscore("delay-queue", 0, time.time(), start=0, num=1) if not values:
		if not values:
			time.sleep(1) # 延时队列空的，休息 1s
			continue
		value = values[0] # 拿第一条，也只有一条
		success = redis.zrem("delay-queue", value) # 从消息队列中移除该消息
		if success: # 因为有多进程并发的可能，最终只会有一个进程可以抢到消息
			msg = json.loads(value) handle_msg(msg)

```



## 应用 4:四两拨千斤 —— HyperLogLog

PFADD key element [element …]

将任意数量的元素添加到指定的 HyperLogLog 里面。

作为这个命令的副作用， HyperLogLog 内部可能会被更新， 以便反映一个不同的唯一元素估计数量（也即是集合的基数）。

当 PFCOUNT key [key …] 命令作用于单个键时， 返回储存在给定键的 HyperLogLog 的近似基数， 如果键不存在， 那么返回 0 


Redis 提供了 HyperLogLog 数据结构就是用来解决 这种统计问题的。HyperLogLog 提供不精确的去重计数方案，虽然不精确但是也不是非常不 精确，标准误差是 0.81%，这样的精确度已经可以满足上面的 UV 统计需求了。


但是如果我们想知道某一个值是不是已经在 HyperLogLog 结构里面了，它就无能为力 了，它只提供了 pfadd 和 pfcount 方法，没有提供 pfcontains 这种方法。



## 应用 5:层峦叠嶂 —— 布隆过滤器

布隆过滤器 (Bloom Filter)，它就是专门用来解决这种去重问题的。 它在起到去重的同时，在空间上还能节省 90% 以上，只是稍微有那么点不精确，也就是有 一定的误判概率。


布隆过滤器可以理解为一个不怎么精确的 set 结构，当你使用它的 contains 方法判断某 个对象是否存在时，它可能会误判。


## 应用 6:断尾求生 —— 简单限流

系统要限定用户的某个行为在指定的时间里 只能允许发生 N 次，如何使用 Redis 的数据结构来实现这个限流的功能?


```python
# coding: utf8
import time 
import redis
client = redis.StrictRedis()
def is_action_allowed(user_id, action_key, period, max_count): 
	key = 'hist:%s:%s' % (user_id, action_key)
	now_ts = int(time.time() * 1000) # 毫秒时间戳
	with client.pipeline() as pipe: # client 是 StrictRedis 实例
		# 记录行为
		pipe.zadd(key, now_ts, now_ts) # value 和 score 都使用毫秒时间戳 
		# 移除时间窗口之前的行为记录，剩下的都是时间窗口内的 
		pipe.zremrangebyscore(key, 0, now_ts - period * 1000)
		# 获取窗口内的行为数量
		pipe.zcard(key)
		# 设置 zset 过期时间，避免冷用户持续占用内存 
		# 过期时间应该等于时间窗口的长度，再多宽限 1s 
		pipe.expire(key, period + 1)
		# 批量执行
		_, _, current_count, _ = pipe.execute()
	# 比较数量是否超标
	return current_count <= max_count

```


## 应用 7:一毛不拔 —— 漏斗限流

单机版

```python

# coding: utf8

import time

class Funnel(object):
	def __init__(self, capacity, leaking_rate):
		self.capacity = capacity # 漏斗容量 
		self.leaking_rate = leaking_rate # 漏嘴流水速率
		self.left_quota = capacity # 漏斗剩余空间 
		self.leaking_ts = time.time() # 上一次漏水时间
	
	def make_space(self):
		now_ts = time.time()
		delta_ts = now_ts - self.leaking_ts # 距离上一次漏水过去了多久 
		delta_quota = delta_ts * self.leaking_rate # 又可以腾出不少空间了 
		if delta_quota < 1: # 腾的空间太少，那就等下次吧
			return
		self.left_quota += delta_quota # 增加剩余空间 
		self.leaking_ts = now_ts # 记录漏水时间
		if self.left_quota > self.capacity: # 剩余空间不得高于容量
			self.left_quota = self.capacity
	
	def watering(self, quota): 
		self.make_space()
		if self.left_quota >= quota: 
			self.left_quota -= quota 
			return True
	return False
```

分布式版

Redis 4.0 提供了一个限流 Redis 模块，它叫 redis-cell。该模块也使用了漏斗算法，并 提供了原子的限流指令




## 应用 8:近水楼台 —— GeoHash


## 应用 9:大海捞针 —— Scan


## 原理 3:未雨绸缪 —— 持久化

**Redis 的持久化机制有两种**

第一种是快照，第二种是 AOF 日志。

1. 快照是一次全量备 份，AOF 日志是连续的增量备份。
2. 快照是内存数据的二进制序列化形式，在存储上非常紧凑，而 AOF 日志记录的是内存数据修改的指令记录文本


**持久化与客户端请求同时处理**

1. fork出子进程来做持久化，主进程处理用户请求，此时子进程和父进程拥有相同的数据
2. 当父进程修改数据的时候，通过COW(Copy On Write) 机制copy一份即将要修的数据页，然后再修改


注：

操作系统的 COW 机制来进行数据段页面的分离。数据段是由很多操 作系统的页面组合而成，当父进程对其中一个页面的数据进行修改时，会将被共享的页面复 制一份分离出来，然后对这个复制的页面进行修改。这时子进程相应的页面是没有变化的， 还是进程产生时那一瞬间的数据。


**AOF 重写**
Redis 提供了 bgrewriteaof 指令用于对 AOF 日志进行瘦身。其原理就是开辟一个子进 程对内存进行遍历转换成一系列 Redis 的操作指令，序列化到一个新的 AOF 日志文件中。

**Redis 4.0 混合持久化**

将 rdb 文 件的内容和增量的 AOF 日志文件存在一起。这里的 AOF 日志不再是全量的日志，而是自 持久化开始到持久化结束的这段时间发生的增量 AOF 日志





## 原理 4:雷厉风行 —— 管道

(Pipeline) 本身并不是 Redis 服务器直接提供的技术，这个技术本质上是由客户端提供的，



## 原理 5:同舟共济 —— 事务


WATCH key [key …]

监视一个(或多个) key ，如果在事务执行之前这个(或这些) key 被其他命令所改动，那么事务将被打断。


```shell
> watch books OK
> incr books (integer) 1
# 被修改了
> multi
OK
> incr books
QUEUED
> exec # 事务执行失败 (nil)
```


## 原理 6:小道消息 —— PubSub

## 原理 7:开源节流 —— 小对象压缩

## 原理 8:有备无患 —— 主从同步

**增量同步**

Redis 同步的是指令流，主节点会将那些对自己的状态产生修改性影响的指令记录在本 地的内存 buffer 中，然后异步将 buffer 中的指令同步到从节点，从节点一边执行同步的指 令流来达到和主节点一样的状态，一遍向主节点反馈自己同步到哪里了 (偏移量)。

因为内存的 buffer 是有限的，所以 Redis 主库不能将所有的指令都记录在内存 buffer 中。Redis 的复制内存 buffer 是一个定长的环形数组，如果数组内容满了，就会从头开始覆 盖前面的内容。


**快照同步**
首先需要在主库上进行一次 bgsave 将当前内 存的数据全部快照到磁盘文件中，然后再将快照文件的内容全部传送到从节点。从节点将快照文件接受完毕后，立即执行一次全量加载，加载之前先要将当前内存的数据清空。加载完 毕后通知主节点继续进行增量同步。




## 集群 1:李代桃僵 —— Sentinel

我们可以将 Redis Sentinel 集群看成是一个 ZooKeeper 集群，它是集群高可用的心脏，它一般是由 3\~5 个节点组成，这样挂了个别节点集群还可以正常运转。

它负责持续监控主从节点的健康，当主节点挂掉时，自动选择一个最优的从节点切换为 主节点。客户端来连接集群时，会首先连接 sentinel，通过 sentinel 来查询主节点的地址， 然后再去连接主节点进行数据交互。如此应用程序将无需重启即可自动完成节点切换。 

## 集群 2:分而治之 —— Codis

## 集群 3:众志成城 —— Cluster

## 拓展 1:耳听八方 —— Stream



## 拓展 3:拾遗漏补 —— 再谈分布式锁

redis的主从同步并不是强一致的，索引在申请锁的时候，主节点挂了，从节点没有同步到锁，从节点成主节点。

**Redlock 算法**

同很多分布式算法一样，redlock 也使用「大多数机制」。
加锁时，它会向过半节点发送 set(key, value, nx=True, ex=xxx) 指令，只要过半节点 set 成功，那就认为加锁成功。




## 拓展 4:朝生暮死 —— 过期策略

**过期的 key 集合**

这只ttl的key的指针被存放在过期字典中

**定时扫描策略**

定时删除是集中处理：默认每秒进行十次过期扫描，过期扫描不会遍历过期字典中所有的 key，而是采用了一种简单的贪心策略。

1. 从过期字典中随机 20 个 key;
2. 删除这 20 个 key 中已经过期的 key; 
3. 如果过期的 key 比率超过 1/4，那就重复步骤 1;

**惰性策略**

惰性删除是零散处理：在访问key的是判断是否过期

**从库的过期策略**

从库不会进行过期扫描，从库对过期的处理是被动的。主库在 key 到期时，会在 AOF 文件里增加一条 del 指令，同步到所有的从库，从库通过执行这条 del 指令来删除过期的 key。





## 拓展 5:优胜劣汰 —— LRU

当物理内存不足时，内存会频繁出现交换（swap），从而影响性能，redis通过内存主动淘汰的办法，避免出现内存交换。

设定最大使用maxmemory，超过此值之后，就行内存淘汰，

**淘汰策略**

- noeviction: 禁止写入
- volatile-lru： 对有过期时间的key，最少使用的淘汰掉
- volatile-ttl： 对有过期时间的key，ttl最小的淘汰掉
- volatile-random：对有过期时间的key，随机删除
- allkeys-lru： 所有key，最少使用的淘汰
- allkeys-random： 所有key，随机删除
- volatile-xxx

**近似 LRU 算法**

1. 位没有一个key增加一个lru的字段，存放最后一次被访问的时间戳
2. 随机采样5个key，淘汰掉最下lru最小的key。
3. 如果使用内存还是大于maxmemory，执行第2步，直到内存低于 maxmemory 为止。


注：

1. 只在写数据的时候可能会触发淘汰
2. 常规的lru是通过链表来实现，最后被访问的放在链表的头部



## 拓展 6:平波缓进 —— 懒惰删除




## 源码 1:极度深寒 —— 探索「字符串」内部结构


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






## 源码 2:极度深寒 —— 探索「字典」内部

## 源码 3:极度深寒 —— 探索「压缩列表」内部

## 源码 4:极度深寒 —— 探索「快速列表」内部

## 源码 5:极度深寒 —— 探索「跳跃列表」内部结构

## 源码 6:极度深寒 —— 探索「紧凑列表」内部

## 源码 7:极度深寒 —— 探索「基数树」内部














