# 微服务

### 为什么需要追踪
### 链路追踪
### 协议
### 序列化
### 负载
### 监控
### 限流
https://en.wikipedia.org/wiki/Token_bucket
https://en.wikipedia.org/wiki/Token_bucket
https://en.wikipedia.org/wiki/Traffic_shaping
https://en.wikipedia.org/wiki/Semaphore_(programming)

### 熔断
和限流器对依赖服务的保护机制不一样，熔断器是当依赖的服务已经出现故障时，为了保证自身服务的正常运行不再访问依赖的服务，防止雪崩效应

熔断器有三种状态：
> 关闭状态：服务正常，并维护一个失败率统计，当失败率达到阀值时，转到开启状态
> 开启状态：服务异常，调用 fallback 函数，一段时间之后，进入半开启状态
> 半开启装态：尝试恢复服务，失败率高于阀值，进入开启状态，低于阀值，进入关闭状态


### 降级


### 过滤
[结合 Guava 源码解读布隆过滤器](https://www.cyhone.com/articles/introduction-of-bloomfilter/)
BloomFilter 的整体思想并不复杂，主要是使用 k 个 Hash 函数将元素映射到位向量的 k 个位置上面，并将这 k 个位置全部置为 1。当查找某元素是否存在时，查找该元素所对应的 k 位是否全部为 1 即可说明该元素是否存在。

*要点：*
1. benign删除元素
2. 错误原因是
3. 错误率：




### 鉴权、统计
### 服务发现
### Istio
[开源技术 * IBM 微讲堂:Istio 系列](https://developer.ibm.com/cn/os-academy-istio/)




## 限流
* 滑动窗口法
* Token Bucket:

令牌桶就是想象有一个固定大小的桶，系统会以恒定速率向桶中放 Token，桶满则暂时不放。
而用户则从桶中取 Token，如果有剩余 Token 就可以一直取。如果没有剩余 Token，则需要等到系统中被放置了 Token 才行

* Leaky Bucket



https://colobu.com/2020/01/21/benchmark-2019-spring-of-popular-rpc-frameworks/

go-micro
grpc
rpcx
gostd_rpc
hprose


