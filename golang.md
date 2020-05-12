# golang


[toc]


## goroutine

### gpm
**g**：goruntine

**p**：processs，是一个逻辑处理单元，最终任务要交个他所绑定的m去处理

1. p拥有一个goruntine本地队列，绑定一个m，还还有内存缓存
2. 通过抢占式调度的办法将本地队列中的g交个m去处理。
3. 如果正在执行的g中超时（>10ms），会发生调度，当前G被被放到本地队列末尾
4. 如果G被阻塞在某个channel操作或network I/O操作上时，G会被放置到某个wait队列中，当I/O available或channel操作完成，在wait队列中的G会被唤醒，标记为runnable，放入到某P的队列中，绑定一个M继续执行
5. 如果G被阻塞在系统调用上，那么不光G会阻塞，执行该G的M也会解绑P(实质是被sysmon抢走了)，与G一起进入sleep状态

**m**：machine，是线程的描述

**wait队列**

如果G被阻塞在某个channel操作或network I/O操作上时，G会被放置到某个wait队列中

**global队列**

[todo]


**sysmon线程**

1. 释放闲置超过5分钟的span物理内存；
2. 如果超过2分钟没有垃圾回收，强制执行；
3. 将长时间未处理的netpoll结果添加到任务队列；
4. 向长时间运行的G任务发出抢占调度；
3. 收回因syscall长时间阻塞的P；

**抢占试调度**

最小中断的单元是函数，是通过在函数的入口处埋点，每次执行函数先检查是否需要中断

**网络io**

Go runtime已经实现了netpoller，这使得即便G发起网络I/O操作也不会导致M被阻塞（仅阻塞G），从而不会导致大量M被创建出来。

**文件io**

regular file的I/O操作一旦阻塞，那么M将进入sleep状态，等待I/O返回后被唤醒；这种情况下P将与sleep的M分离，再选择一个idle的M。如果此时没有idle的M，则会新创建一个M，这就是为何大量I/O操作导致大量Thread被创建的原因。


### gm
**g**：goruntine

**m**：machine，是线程的描述

用于一个有锁的全局goruntine队列

1. 单一全局互斥锁(Sched.Lock)和集中状态存储的存在导致所有goroutine相关操作，比如：创建、重新调度等都要上锁；
2. goroutine传递问题：M经常在M之间传递”可运行”的goroutine，这导致调度延迟增大以及额外的性能损耗；
3. 每个M做内存缓存，导致内存占用过高，数据局部性较差；
4. 由于syscall调用而形成的剧烈的worker thread阻塞和解除阻塞，导致额外的性能损耗。


**参考资料**

1. [也谈goroutine调度器](https://tonybai.com/2017/06/23/an-intro-about-goroutine-scheduler/)  
2. [图解Go运行时调度器](https://tonybai.com/2020/03/21/illustrated-tales-of-go-runtime-scheduler/)  



### netpoller



## 基本类型

### channel
是一个有锁的循环队列，由于缺乏数据而等待的goruntine队列：sendq，recvq

**数据结构**

1. 数据buf，队列大小dataqsiz，元素个数qcount
- 读写指针readx，sendx
- 读写等待队列，reavq，sendq，用于存储由于缺乏数据而阻塞的goruntine

**发送过程**

1. 当存在等待的接收者（recvq）时，通过 runtime.send 直接将数据发送给阻塞的接收者；
- 当缓冲区存在空余空间时，将发送的数据写入 Channel 的缓冲区；
- 当不存在缓冲区或者缓冲区已满时，等待其他 Goroutine 从 Channel 接收数据；并且当前goruntine加入sendq

**接收过程**

1. 当存在等待的发送者时（sendq），通过 runtime.recv 直接从阻塞的发送者或者缓冲区中获取数据；
- 当缓冲区存在数据时，从 Channel 的缓冲区中接收数据；
- 当缓冲区中不存在数据时，等待其他 Goroutine 向 Channel 发送数据，并且当前goruntine加入recvq


**参考资料**

1. [6.4 Channel](https://draveness.me/golang/docs/part3-runtime/ch06-concurrency/golang-channel/)





### map

通过拉链法实现

**数据结构**

```
type hmap struct {
    // 元素个数，调用 len(map) 时，直接返回此值
    count     int
    flags     uint8
    // buckets 的对数 log_2
    B         uint8
    // overflow 的 bucket 近似数
    noverflow uint16
    // 计算 key 的哈希的时候会传入哈希函数
    hash0     uint32
    // 指向 buckets 数组，大小为 2^B
    // 如果元素个数为0，就为 nil
    buckets    unsafe.Pointer
    // 扩容的时候，buckets 长度会是 oldbuckets 的两倍
    oldbuckets unsafe.Pointer
    // 指示扩容进度，小于此地址的 buckets 迁移完成
    nevacuate  uintptr
    extra *mapextra // optional fields
}

type bmap struct {
    topbits  [8]uint8
    keys     [8]keytype
    values   [8]valuetype
    pad      uintptr
    overflow uintptr
}

type mapextra struct {
    // overflow[0] contains overflow buckets for hmap.buckets.
    // overflow[1] contains overflow buckets for hmap.oldbuckets.
    overflow [2]*[]*bmap

    // nextOverflow 包含空闲的 overflow bucket，这是预分配的 bucket
    nextOverflow *bmap
}
```

**原理**

1. 保留两个bucket数组指针，分别是buckets， oldbuckets，
- 通过hash(key)的低B位来索引到buckets数组的链表
- 每个bucket存储8个KV，
- bucket顶部重复每个key的hash(key)的高8位，
- key，value分开独立存储，


**扩容方式**

- rehash
- 压缩bucket


**扩容时机**

在向 map 插入新 key 的时候

**扩容条件**

1. 装载因子(count/len(bucket))超过阈值(6.5)
2. overflow 的 bucket 数量过多：

```shell
    当 B < 15，也就是 bucket 总数 2^B 小于 2^15 时，如果 overflow 的 bucket 数量超过 2^B；    
    当 B >= 15，也就是 bucket 总数 2^B 大于等于 2^15，如果 overflow 的 bucket 数量超过 2^15
```

```go
    if B < 15 && count(overflow) > 2^B {        
    } else if B >= 15 && count(overflow) > 2^15 {        
    }
```

**rehash的过程**

[todo]



**参考**

1. [3.3 哈希表](https://draveness.me/golang/docs/part2-foundation/ch03-datastructure/golang-hashmap/)
- [map 的底层实现原理是什么](https://qcrao91.gitbook.io/go/map/map-de-di-ceng-shi-xian-yuan-li-shi-shi-mo)
- [map 的扩容过程是怎样的](https://qcrao91.gitbook.io/go/map/map-de-kuo-rong-guo-cheng-shi-zen-yang-de)





### interface

是一个类型包装器，包装真实的类型和数据，还有被实现的接口函数

**带方法的接口**

```go
type eface struct { // 16 bytes
    _type *_type
    data  unsafe.Pointer
}
type _type struct {
    size       uintptr
    ptrdata    uintptr
    hash       uint32
    tflag      tflag
    align      uint8
    fieldAlign uint8
    kind       uint8
    equal      func(unsafe.Pointer, unsafe.Pointer) bool
    gcdata     *byte
    str        nameOff
    ptrToThis  typeOff
}
```

**不带方法的接口**

```go
type iface struct { // 16 bytes
    tab  *itab
    data unsafe.Pointer
}
type itab struct { // 32 bytes
    inter *interfacetype
    _type *_type
    hash  uint32
    _     [4]byte
    fun   [1]uintptr
}
```

**注意**

1. Go 语言只会在传递参数、返回参数以及变量赋值时才会对某个类型是否实现接口进行检查，
2. 与 C 语言中的 void * 不同，interface{} 类型不是任意类型
3. 指针和值类型都可以实现接口的，但是不能同时实现同一个接口；对象都是通过值传递的
4. 指针可以使用值的接口，但是不可以使用指针的接口


**参考**

1. [4.2 接口](https://draveness.me/golang/docs/part2-foundation/ch04-basic/golang-interface/)
2. [iface 和 eface 的区别是什么](https://qcrao91.gitbook.io/go/interface/iface-he-eface-de-qu-bie-shi-shi-mo)



### slice


### array

与很多语言不同，Go 语言中数组在初始化之后大小就无法改变，存储元素类型相同、但是大小不同的数组类型在 Go 语言看来也是完全不同的，只有两个条件都相同才是同一个类型。


另一种是使用 [...]T 声明数组，


### select



### defer


## 标准库

### sync.Map

```go
type Map struct {
	mu Mutex
	read atomic.Value // readOnly
	dirty map[interface{}]*entry
	misses int
}

type readOnly struct {
    m  map[interface{}]*entry
    amended bool 
}

type entry struct {
    //可见value是个指针类型，虽然read和dirty存在冗余情况（amended=false），但是由于是指针类型，存储的空间应该不是问题
    p unsafe.Pointer // *interface{}
}
```

**原理**

1. 读写分离的，read存放只读数据，dirty存放被修改过的数据，数据通过指针共享，没有重复的数据
- 读失败一定测次数智慧，就dirty中的数据同步到read中，清空dirty
- 增（改）数据：1.修改read，2.修改dirty，3.向dirty中增加新数据，如果dirty为nil，需要先从read中同步数据到dirty中


### golang.org/x/time/rate


### context.Context



### atomic.Value


### unsafe.Pointer




## 内存
### gc：并发三色标记，写屏障，并发清除
[培训大师 William 图解 Go 中的 GC 系列：第一部分 - 基础](https://mp.weixin.qq.com/s/mYp3QbdWR4HEZimFUw9bAA).  
[培训大师 William 图解 Go 中的 GC 系列：第二部分 - GC 追踪](https://mp.weixin.qq.com/s?__biz=MzAxMTA4Njc0OQ==&mid=2651437958&idx=2&sn=f5cc599c2da7c463ec26750cb352057c&chksm=80bb6574b7ccec6210b534c11896abb61f81f9117675feb8a70b696e850cde2a0cf936fca4b9&scene=21#wechat_redirect).  
[培训大师 William 图解 Go 中的 GC 系列：第三部 - GCPacing](https://mp.weixin.qq.com/s?__biz=MzAxMTA4Njc0OQ==&amp;mid=2651438002&amp;idx=1&amp;sn=5e46b5fce65ba65a0ed53256326122ed&amp;chksm=80bb6540b7ccec56171516ba721150ab4a44117043f157e263cbaccf3ff11805bb96fb644e8e#rd).  




### memory allocation
[可视化Go内存管理](https://tonybai.com/2020/03/10/visualizing-memory-management-in-golang/)   
[Go 内存分配器可视化指南](https://www.linuxzen.com/go-memory-allocator-visual-guide.html)   
[7.1 内存分配器](https://draveness.me/golang/docs/part3-runtime/ch07-memory/golang-memory-allocator/)

程序中的数据和变量都会被分配到程序所在的虚拟内存中，内存空间包含两个重要区域 — 栈区（Stack）和堆区（Heap）。函数调用的参数、返回值以及局部变量大都会被分配到栈上，这部分内存会由编译器进行管理；


spans 区域存储了指向内存管理单元 runtime.mspan 的指针，每个内存单元会管理几页的内存空间，每页大小为 8KB；


http://btfak.com/golang/2017/02/08/golang-memory-allocate/


### 内存逃逸


## tools
- go build
- go tool compile
- gdb
- GODEBUG
```
GODEBUG=schedtrace=DURATION
```
- profiling



## 基本用法
- sync.Once
- sync.Pool
- sync.atomic

- select
- switch


## 细节
[Go语言101](https://gfw.go101.org/article/101.html)  
time.After资源释放

```golang
// After waits for the duration to elapse and then sends the current time
// on the returned channel.
// It is equivalent to NewTimer(d).C.
// The underlying Timer is not recovered by the garbage collector
// until the timer fires. If efficiency is a concern, use NewTimer
// instead and call Timer.Stop if the timer is no longer needed.
func After(d Duration) <-chan Time {
    return NewTimer(d).C
}
```


## 常问问题
- gpm模型说一下，gpm为什么比gm模型好
- go程序最少，最多起多少个线程
- gc的过程说一下
- recover：应该放在什么地方，可以不放在defer中吗
- make返回的是指针还是引用
- select中的分支返回的顺序
- 一个goruntime最少消耗多少内存
8K的栈空间


## 新变化
系统信号的异步抢占调度
[Go 1.14中值得关注的几个变化](https://tonybai.com/2020/03/08/some-changes-in-go-1-14/)
[关于Go1.14，你一定想知道的性能提升与新特性](https://juejin.im/post/5e3f9990e51d4526cc3b1672)




## web框架
### gin

### fasthttp



## 参考
[Go-Questions](https://qcrao91.gitbook.io/go/)
[Go 语言设计与实现](https://draveness.me/golang/)
[Go 语言原本](https://changkun.de/golang/)
[](https://www.youtube.com/channel/UCZwrjDu5Rf6O_CX2CVx7n8Q/videos)
[](https://github.com/talk-go/night)
[](https://talkgo.org/)
[](https://github.com/talk-go/read)
[](https://talkgo.org/interview/)
[](https://draveness.me/golang/)
[](https://learnku.com/docs/go-blog)
[](https://github.com/qichengzx/gopher-reading-list-zh_CN)
[](https://blog.go-zh.org/)
[](http://legendtkl.com/)
[](https://github.com/hyper0x/go_command_tutorial)
[](https://github.com/gopherchina/conference)
[Go语言高级编程](https://chai2010.cn/advanced-go-programming-book/)





这个桶的结构体 bmap 在 Go 语言源代码中的定义只包含一个简单的 tophash 字段，tophash 存储了键的哈希的高 8 位，通过比较不同键的哈希的高 8 位可以减少访问键值对次数以提高性能：

bmap 结构体其实不止包含 tophash 字段




当调用此函数的时候，可以传入任意类型，只要它实现了 say_hello() 函数就可以。如果没有实现，运行过程中会出现错误。

那在编译阶段就不会通过。这也是静态语言比动态语言更安全的原因

一个对象有效的语义，不是由继承自特定的类或实现特定的接口，而是由它"当前方法和属性的集合"决定。



