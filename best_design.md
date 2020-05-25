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


## 秒杀

