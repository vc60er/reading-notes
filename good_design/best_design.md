# 优秀的设计



<!-- MarkdownTOC autolink=true -->

- [Go开源项目BigCache如何加速并发访问以及避免高额的GC开销](#go%E5%BC%80%E6%BA%90%E9%A1%B9%E7%9B%AEbigcache%E5%A6%82%E4%BD%95%E5%8A%A0%E9%80%9F%E5%B9%B6%E5%8F%91%E8%AE%BF%E9%97%AE%E4%BB%A5%E5%8F%8A%E9%81%BF%E5%85%8D%E9%AB%98%E9%A2%9D%E7%9A%84gc%E5%BC%80%E9%94%80)
- [定时器](#%E5%AE%9A%E6%97%B6%E5%99%A8)
	- [时间轮](#%E6%97%B6%E9%97%B4%E8%BD%AE)
	- [最小堆](#%E6%9C%80%E5%B0%8F%E5%A0%86)
- [延迟队列](#%E5%BB%B6%E8%BF%9F%E9%98%9F%E5%88%97)
- [自动完成](#%E8%87%AA%E5%8A%A8%E5%AE%8C%E6%88%90)
	- [Auto Complete with Redis： use zset](#auto-complete-with-redis%EF%BC%9A-use-zset)
- [秒杀](#%E7%A7%92%E6%9D%80)
- [限流](#%E9%99%90%E6%B5%81)
	- [计数器](#%E8%AE%A1%E6%95%B0%E5%99%A8)
	- [漏斗](#%E6%BC%8F%E6%96%97)
	- [令牌桶](#%E4%BB%A4%E7%89%8C%E6%A1%B6)
	- [滑动窗口](#%E6%BB%91%E5%8A%A8%E7%AA%97%E5%8F%A3)
	- [管道](#%E7%AE%A1%E9%81%93)
- [降级](#%E9%99%8D%E7%BA%A7)
- [流量整形](#%E6%B5%81%E9%87%8F%E6%95%B4%E5%BD%A2)
- [数据库](#%E6%95%B0%E6%8D%AE%E5%BA%93)
	- [树结构](#%E6%A0%91%E7%BB%93%E6%9E%84)
	- [长字符串索引](#%E9%95%BF%E5%AD%97%E7%AC%A6%E4%B8%B2%E7%B4%A2%E5%BC%95)

<!-- /MarkdownTOC -->


## Go开源项目BigCache如何加速并发访问以及避免高额的GC开销

<https://pengrl.com/p/35302/>

BigCache是一个优秀的内存缓存库，使用了以下方法

1. 将key通过hash sharding到多个map中，减少了锁的粒度
- 就是把map的key和value都弄成了无指针类型，这样gc的过程中就不会遍历map。
	具体做法如下： 将value序列化成[]byte, 再将[]byte打包追加到全局的[]byte中，然后把写入位置当作value计入map中
- 计算key的hash值，当作map的key
- 使用标记删除的方法删除元素
- map中的key可能发生冲突



## 定时器
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
3. 过滤掉没有结尾标记（\*）词汇

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
算法：
1. 对一定时间内的请求计数
2. 判断一定时间内的请求总是是否大于阀值，如果大于，则拒绝，小于则通过
缺点：
1. 会出现断流和锯齿的情况
2. 不能有效压制峰值

### 漏斗

### 令牌桶

### 滑动窗口

### 管道


## 降级

## 流量整形


## 数据库
### 树结构
### 长字符串索引




