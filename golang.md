# golang


[toc]


## 底层实现

### goruntime：gpm，gm
[也谈goroutine调度器](https://tonybai.com/2017/06/23/an-intro-about-goroutine-scheduler/)
[图解Go运行时调度器](https://tonybai.com/2020/03/21/illustrated-tales-of-go-runtime-scheduler/)

### gc：并发三色标记，写屏障，并发清除
[培训大师 William 图解 Go 中的 GC 系列：第一部分 - 基础](https://mp.weixin.qq.com/s/mYp3QbdWR4HEZimFUw9bAA).  
[培训大师 William 图解 Go 中的 GC 系列：第二部分 - GC 追踪](https://mp.weixin.qq.com/s?__biz=MzAxMTA4Njc0OQ==&mid=2651437958&idx=2&sn=f5cc599c2da7c463ec26750cb352057c&chksm=80bb6574b7ccec6210b534c11896abb61f81f9117675feb8a70b696e850cde2a0cf936fca4b9&scene=21#wechat_redirect).  
[培训大师 William 图解 Go 中的 GC 系列：第三部 - GCPacing](https://mp.weixin.qq.com/s?__biz=MzAxMTA4Njc0OQ==&amp;mid=2651438002&amp;idx=1&amp;sn=5e46b5fce65ba65a0ed53256326122ed&amp;chksm=80bb6540b7ccec56171516ba721150ab4a44117043f157e263cbaccf3ff11805bb96fb644e8e#rd).  


### channel：有锁的循环队列
[6.4 Channel](https://draveness.me/golang/docs/part3-runtime/ch06-concurrency/golang-channel/)

#### 数据结构  
- 包含数据buf，队列大小dataqsiz，元素个数qcount
- 包含读写指针readx，sendx
- 读写等待队列，reavq，sendq，用于存储由于缺乏数据而阻塞的goruntine

#### 发送过程
- 当存在等待的接收者（recvq）时，通过 runtime.send 直接将数据发送给阻塞的接收者；
- 当缓冲区存在空余空间时，将发送的数据写入 Channel 的缓冲区；
- 当不存在缓冲区或者缓冲区已满时，等待其他 Goroutine 从 Channel 接收数据；并且当前goruntine加入sendq

#### 接受过程
- 当存在等待的发送者时（sendq），通过 runtime.recv 直接从阻塞的发送者或者缓冲区中获取数据；
- 当缓冲区存在数据时，从 Channel 的缓冲区中接收数据；
- 当缓冲区中不存在数据时，等待其他 Goroutine 向 Channel 发送数据，并且当前goruntine加入recvq


Channel 在不同的控制结构中组合使用时的现象


### map
[3.3 哈希表](https://draveness.me/golang/docs/part2-foundation/ch03-datastructure/golang-hashmap/)

通过hash数据计算hash值，更加hash值将数据sharding到数据中。
两种实现方法
开放寻址法：只有数组，冲突后，探测数组中的下一个索引的数据
拉链法：数组加上链表实现，冲突后，连接到连标上

开放寻址法中对性能影响最大的就是装载因子，它是数组中元素的数量与数组大小的比值


#### 数据结构

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

每个bucket存储8个kv
通过hash(key)的低B位来索引bucket数组，


#### 扩容方式
- rehash
- 压缩bucket

装载因子超过阈值，源码里定义的阈值是 6.5。
overflow 的 bucket 数量过多：当 B 小于 15，也就是 bucket 总数 2^B 小于 2^15 时，如果 overflow 的 bucket 数量超过 2^B；当 B >= 15，也就是 bucket 总数 2^B 大于等于 2^15，如果 overflow 的 bucket 数量超过 2^15。






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
- 读写分离的，read存放只读数据，dirty存放被修改过的数据，数据通过指针共享，没有重复的数据
- 读失败一定测次数智慧，就dirty中的数据同步到read中，清空dirty
- 增（改）数据：1.修改read，2.修改dirty，3，向dirty中增加新数据，如果dirty为nil，需要先从read中同步数据到dirty中





### atomic.Value


### slice
### select
### interface
不过 Go 语言中有两种略微不同的接口，一种是带有一组方法的接口，另一种是不带任何方法的 interface{}：
Go 语言只会在传递参数、返回参数以及变量赋值时才会对某个类型是否实现接口进行检查，
不过 Go 语言中有两种略微不同的接口，一种是带有一组方法的接口，另一种是不带任何方法的 interface{}：
与 C 语言中的 void * 不同，interface{} 类型不是任意类型，
Go 语言的接口类型不是任意类型





### unsafe.Pointer

### array

与很多语言不同，Go 语言中数组在初始化之后大小就无法改变，存储元素类型相同、但是大小不同的数组类型在 Go 语言看来也是完全不同的，只有两个条件都相同才是同一个类型。


另一种是使用 [...]T 声明数组，



### memory allocation
[可视化Go内存管理](https://tonybai.com/2020/03/10/visualizing-memory-management-in-golang/)
[Go 内存分配器可视化指南](https://www.linuxzen.com/go-memory-allocator-visual-guide.html)
[7.1 内存分配器](https://draveness.me/golang/docs/part3-runtime/ch07-memory/golang-memory-allocator/)

程序中的数据和变量都会被分配到程序所在的虚拟内存中，内存空间包含两个重要区域 — 栈区（Stack）和堆区（Heap）。函数调用的参数、返回值以及局部变量大都会被分配到栈上，这部分内存会由编译器进行管理；


spans 区域存储了指向内存管理单元 runtime.mspan 的指针，每个内存单元会管理几页的内存空间，每页大小为 8KB；




### 内存逃逸

### tools
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
// After waits for the duration to elapse and then sends the current time
// on the returned channel.
// It is equivalent to NewTimer(d).C.
// The underlying Timer is not recovered by the garbage collector
// until the timer fires. If efficiency is a concern, use NewTimer
// instead and call Timer.Stop if the timer is no longer needed.
func After(d Duration) <-chan Time {
    return NewTimer(d).C
}



## 常问问题
- gpm模型说一下，gpm为什么比gm模型好
- go程序最少，最多起多少个线程
- gc的过程说一下
- recover：应该放在什么地方，可以不放在defer中吗
- make返回的是指针还是引用
- select中的分支返回的顺序
- 一个goruntime最少消耗多少内存
8K的栈空间


## 
系统信号的异步抢占调度
[Go 1.14中值得关注的几个变化](https://tonybai.com/2020/03/08/some-changes-in-go-1-14/)


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



