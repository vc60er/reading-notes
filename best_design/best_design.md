# 优秀的设计


## [Go开源项目BigCache如何加速并发访问以及避免高额的GC开销](https://pengrl.com/p/35302/)

BigCache是一个优秀的内存缓存库，使用了以下方法

1. 将key通过hash sharding到多个map中，减少了锁的粒度
- 就是把map的key和value都弄成了无指针类型，这样gc的过程中就不会遍历map。
	具体做法如下： 将value序列化成[]byte, 再将[]byte打包追加到全局的[]byte中，然后把写入位置当作value计入map中
- 计算key的hash值，当作map的key
- 使用标记删除的方法删除元素
- map中的key可能发生冲突



## 定时起
### 时间轮
### 最小堆



## 延迟队列



## 自动完成
### Auto Complete with Redis： use zset

<http://oldblog.antirez.com/post/autocomplete-with-redis.html>

**准备**

将每个单词的所有前缀zadd 到 zset中，并且给单词结尾添加标记（\*），也zadd到zset中

例如添加单词bar foo foobar 的情况：

```shell
redis> zrange zset 0 -1
1. "b"
2. "ba"
3. "bar"
4. "bar*"
5. "f"
6. "fo"
7. "foo"
8. "foo*"
9. "foob"
10. "fooba"
11. "foobar"
12. "foobar*"
```

**查询**

1. 通过zrank查到关键字的位置
2. 通过zrange 查询与之相关的词汇列表
3. 过滤掉没有解决标记（\*）词汇

例如：

```shell
redis> zrank zset fo
(integer) 5
redis> zrange zset 6 -1
1. "foo"
2. "foo*"
3. "foob"
4. "fooba"
5. "foobar"
6. "foobar*"

# 返回 foo foobar
```


**分析**

Both ZRANK and ZRANGE (with a fixed range of 50 elements) are O(log(N)) operations. So we really don't have problems about handling huge word lists composed of millions of elements.


Fortunately in the practice there are a lot of collisions among prefixes, so this is going to be better. For the 4960 female names we needed 14798 elements in the sorted set.



**改进**

Query prediciton

Given a prefix you want to find the first N words having this prefix ordered by frequency, that is, if I'm typing "ne" I want to see things like "netflix", "news", "new york times"

Clean up stage

Then using RANDOMKEY you can get a random element from time to time with a background worker, and check if it's better to delete it since it was updated a few days ago the last time.



未完




## 秒杀

## 限流
### 计数器
### 管道
### 令牌桶
### 漏斗


## 降级



