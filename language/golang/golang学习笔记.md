# Golang 的底层实现

## 调度器

### 整体架构（GPM）
- G：协程的描述符，内部包括协程的栈，以及上下文信息

  - ```
    todo
    ```

  - 

- P：逻辑执行单元， 数量一般 cpu 数一致，内部包含可运行的 G 队列（容量 256），以及定时器堆

  - ```
    结构如下：
    P
    ├── runnext       一个 G 指针（优先运行槽位）
    └── runq          256 个 G 的环形队列
        ├── runqhead  队首
        └── runqtail  队尾
    ```

- M：线程的描述服务，内部包含一个 g0 协程，用于执行调度逻辑

  - ```
    todo
    ```

  - 

- 全局协程可运行队列：提供一个跨 P 共享的任务池，实现任务分配与调度公平性。

  - 



![Image 1: img{512x368}](https://tonybai.com/images/wp-content/uploads/goroutine-scheduler-model.png)



### 协程中断条件
1. 阻塞
   - 主动休眠
   - 锁阻塞
   - 网络I/O阻塞
   - 管道阻塞

2. 系统调用

3. 时间片调度



### 时间片调度方法

- 超期发现：

  sysmon 周期检查 P 的调度进展，发现调度进度长期未变化，达到约 10ms 阈值。标记 G 的抢占状态，设置栈检查标记:gpstackguard0 == stackPreempt，gp.preempt = true。

- 抢断方式：

  > 先准备同步抢占条件，并在支持时尝试异步抢占；最终哪条路径先成功，取决于执行时机和安全条件。

  - 同步抢断：在函数序言等**栈检查点**发现请求

    - G 正常执行函数。
    - 进入具有栈检查的函数序言，检查栈边界。
    - 发现 `stackguard0 == stackPreempt`。
    - 进入 Runtime 的栈扩展处理路径，识别出这是抢占请求。
    - G 转入调度流程，让出执行权。

  - 异步抢断：通过 OS 信号打断执行，寻找异步安全点

    - 向运行 G 的 M 发送信号（Linux 上通常使用 `SIGURG`）。
    - 信号处理路径检查当前执行位置是否适合异步抢占。
    - 如果当前位置安全，Runtime 保存必要的执行现场，并转入 `asyncPreempt` 抢占处理路径。
    - G 进入调度流程，交出执行权。

    


### 新协程的选择

1. 检查并执行已到期的定时器

   > 到期定时器可能唤醒 G，但不代表一定立即执行该 G。

2. 每 61 次从全局队列中获一次G

   > 更准确地说：当 `pp.schedtick % 61 == 0` 且全局队列非空时，尝试取一个 G，以避免本地队列中的任务长期占据调度机会。

3. 从本地队列中获取G

   > 先检查 `runnext`，再检查普通本地运行队列。

4. 从全局队列中获取一批 G，第一个 G 直接返回执行，其余 G 放入当前 P 的**本地队列**。

5. 从 netpoll 中获取多个就绪的 G，第一个 G 直接返回执行，其他放入全局队列

   > 其余通过 `injectglist()` 分配，不一定全部进入全局队列。

6. 从其他 P 中偷取一半的 G，第一个 G 直接返回执行，其他放入**本地队列**。

   > 具体数量取决于目标 P 的队列和窃取逻辑。



### 协程切换的方法

Go 通过保存和恢复 G 的执行上下文实现协程切换。上下文主要包括栈指针 SP、程序计数器 PC 等必要状态。底层汇编负责恢复执行现场并跳转到新 G 的执行位置。

切换过程有 g0 完成，步骤如下：

1. M 从老 G 的栈切到 g0 的栈，
2. M 执行 g0 上的调度函数runtime.schedule，找到可运行的新 G
3. M 切到新 G 的栈，继续执行



### 老协程的处理方法

1. 阻塞

   - 主动休眠
     - 状态更新为_Gwaiting
     
     - 关联到定时器的堆上
     
     - 超时后，修改状态为_Grunnable，G 赋值给优先调度指针runnext，等待执行
     
       > 如何runnext 被占用，且本地队列已满，则将此 G （还有此队列半数的 G）移动到全队队列
       >
       > runnext指的是，执行唤醒操作的 M 当前持有的 P 的本地队里中的runnext
     
   - 锁阻塞
     - 状态更新为_Gwaiting
     
     - 关联到锁的等待队列
     
     - 解锁时，修改状态为_Grunnable，G赋值给优先调度指针runnext，等待执行
     
       > 如何runnext 被占用，且本地队列已满，则将此 G （还有此队列半数的 G）移动到全队队列
       >
       > 如果有多个等待锁，还需要再次执行抢锁
       >
       > runnext指的是，执行唤醒操作的 M 当前持有的 P 的本地队里中的runnext
     
   - 管道阻塞
     - 状态更新为_Gwaiting
     
     - 关联到到管道的 `sendq` 或 `recvq`
     
     - 另一端读取或者写入时候，从相应的队列中获得G，以及相关数据。修改状态为_Grunnable，G赋值给优先调度指针runnext，等待执行
     
       > 如何runnext 被占用，且本地队列已满，则将此 G （还有此队列半数的 G）移动到全局队列
       >
       > runnext指的是，执行唤醒操作的 M 当前持有的 P 的本地队里中的runnext
     
   - 网络I/O阻塞
     - 状态更新为_Gwaiting
     - 关联到 `netpoll` 的等待集合
     - 调度器找协程时或者监控线程周期调用netpoll()，取出IO就绪的 G 列表 。第一个有调度器执行，剩余移入全局队列（todo，这里写不完整）

2. 系统调用
   - G执行系统调用	
     - G 状态变为 _Gsyscall
     - P 与M  解除关联，M 执行系统调用
     - 原 P 进入 _Psyscall 状态，之后可能被回收
   - 系统调用返回
      - G 恢复为 _Grunning
      - 尝试重新获取原来的 P
        - 成功：M 与 此P 关联，直接继续执行
        - 失败
          - 尝试获取其他空闲 P
            - 成功：M 与 此P 关联，直接继续执行
            - 失败：放入全局运行队列，等待后续调度

3. 执行超期

   1. 状态更新为：Grunnable
   1. 移动到全局队列，等待被调度



### 全局队列的用途
> 实现协程共享，公平调度

1. P 的本地队列已满，有新增 G 时，调度器将其中一半的 G 移动到全局队列
2. P 的本地队列已空，获取新 G 是，从全局队列获取
3. 运行超时的 G中断后，被放入全局队列



### 本地队列的优化

局部本地化的优化原则是，谁产生/唤醒了 G，就尽量让该 G 在同一个 P 上立即执行，而不恢复到中断前的 P 上执行。让“刚通信完的两个 G”尽快连续执行，减少跨 P 迁移和缓存失效 。





## 内存管理

### 整体架构

#### 概念

- arena：一次向系统申请 64M 内存

- page：将 arena 按照 8K 拆分成页

- mspan：由一个或连续的多个页构成 mspan（67 种规格）

- slot：将 mspan 按照它的规格拆分成固定大小的槽，用于对象的分配

  ```go
  type mspan struct {
    	// 该mspan管理的关联的页数，每页8KB
    	npages    uintptr // number of pages in span
    	
    	allocBits  *gcBits  // 记录slot的分配状态
    	// 该mspan负责分配的内存块大小
    	elemsize    uintptr       // computed from sizeclass or from npages
    }
  ```

  


#### 三级内存池

- mcache：P 的本地 mspan 缓存，无锁，负责将 mspan 上的 slot 分配给对象

  > 共个136 mspan，其中 67 个用于包含指针，67 个用于不包含指针，2 个用于大块内存（大于32768字节）

  ```go
  type p struct {
  	mcache 		  *mcache
  }
  type mcache struct {
  	// 共个136 mspan，其中 67 个用于包含指针，67 个用于不包含指针，2 个用于大块内存（大于32768字节）
  	alloc [numSpanClasses]*mspan 
  }
  ```


- mcentral：全局 mspan 池，有锁，管理共享的 mspan 集合，接收用满的 mspan，返回空 mspan

  - 负责构造 mspan，

  - 为 mcache 提供空 mspan，

  - 收纳 mcache 已经写满的 mspan

  

- mheap：管理页及内存布局

  ```go
  var mheap_ mheap
  
  type mheap struct {
      // 共个136，其中 67 个用于包含指针，67 个用于不包含指针，2 个用于大块内存（大于32768字节）
      central [numSpanClasses] struct {
          mcentral mcentral
      }
  }
  type mcentral struct {
      partial   [2]spanSet   // 仍有空闲对象（slot）的 span 集合
      full      [2]spanSet   // 没有空闲对象（slot）的 span 集合
  }
  // 这里的数组长度为 2，是为了区分 已完成 sweep 和 尚未完成 sweep 的 span 集合，并在 GC 周期之间切换角色。
  // 已 sweep 的 partial span： 可以优先寻找并交给 mcache。
  ```



#### 架构

<img src="/Users/apple/Library/Application Support/typora-user-images/image-20261021111816572.png" alt="image-20261021111816572" style="zoom:40%;" />



### 内存分配

#### mcache 上的对象的内存分配过程

1. 根据对象大小确定 `spanClass`。

2. 从 `mcache.alloc[spanClass]` 取得当前 `mspan`。

3. 在 span 的空闲对象位图中找到可用槽位。

4. 标记该槽位已分配，返回对象地址。

   

#### mcache 上的 mspan 分配过程

> 当 mcache 上的某规格的 mspan 的 slot 用完时，需要从 mcentral 分配新的mspan

1. 将旧 span 归还到对应的 mcentral
2. 向 mcentral 获取一个有空闲槽位的 span
3. 把新 span 放进 mcache.alloc[class]

#### mcentral 上的 mspan 分配过程

> mcentralsh 上的 mspan 用完是，从 mheap 中申请新的 mspan

1. todo



#### 示意图

- 场景一：分配一个小对象，mcache 有空位

  ```
  G 在 P 上运行
        │
        ▼
      mcache
        │
        ▼
  找到对应 span class 的 mspan
        │
        ▼
  找到空闲对象槽位
        │
        ▼
  返回对象地址
  ```

  

- 场景二：mcache 当前 span 已满

  - 

  ```text
  mcache
    │
    │ 当前 span 无空位
    ▼
  mcentral
    │
    ├── 找到可用 span ──> 交给 mcache
    │
    └── 找不到可用 span
                │
                ▼
              mheap
                │
                ▼
        分配连续堆页并创建 span
                │
                ▼
            mcentral 接管
                │
                ▼
            mcache 获取 span
                │
                ▼
            继续分配对象
  ```

  

- 场景三：GC 后，span 中出现空闲槽位

  ```text
  GC 标记
     │
     ▼
  sweep span
     │
     ├── span 仍有存活对象
     │        │
     │        ▼
     │   重新进入可复用的 span 管理体系
     │   （通常由对应 mcentral 管理）
     │
     └── span 中所有对象均已释放
              │
              ▼
          span 的堆页归还 mheap
              │
              ▼
          可用于后续堆空间分配
  ```



### 回收（GC）

> Go 使用非移动式、并发标记清扫 GC

#### GC 的核心算法：三色标记

三色标记是一种理解 Go GC 的抽象模型。颜色表示对象在当前标记周期中的处理状态。

- 白色：尚未被标记的对象。标记结束后仍然是白色的堆对象，属于可回收候选。

- 灰色：已经发现，但对象内部的指针还没有全部扫描。通常由标记工作队列保存待扫描任务。

- 黑色：对象已经扫描完成，其内部需要追踪的指针已经处理。

简单来说，将对象初始为白色，扫描后为灰色，子指针也被扫描后为黑色。最后白色对象是可回收的对象

> - 扫描是个耗时过程，灰色是一个中间状态
>
> - 在正常的三色追踪过程中，不允许存在黑色对象直接指向尚未被发现的白色对象，除非相应的并发写屏障机制能够保证该白色对象最终被标记。



#### 概念

- 可达对象： 从 GC Roots 出发，沿指针关系能够到达。
- 不可达对象： 无法从任何 GC Root 到达，可以被回收。
- GC Root： 垃圾回收扫描的起点，主要包括 goroutine 栈上的指针、全局变量，以及运行时需要追踪的其他根引用。



#### 写屏障

- 背景：

  GC 与业务 goroutine 完全串行执行，两者会同时访问、修改对象之间的指针关系。会导致扫描结果和实际有偏差。最终导致错误回收。比如，以下情况：

  - 漏标问题：黑色对象指向白色对象

    > GC 已扫描 A，业务代码随后修改引用，使黑色对象 A 指向尚未标记的白色对象 B；如果 B 原有的待扫描引用路径同时被删除，B 就可能漏标，最终被错误回收。

    - 初始状态：`Root --> A(黑)`，`Root --> C(灰) --> B(白)`
    - 业务修改：`A --> B`，同时删除 `C --> B`
    - 最终状态：`Root --> A(黑) --> B(白)`，B 仍然可达，但 GC 可能无法发现它。

- 本质：

  - 写屏障的本质是编译器与运行时配合，在特定指针写入操作附近执行的额外逻辑。

  - Go 1.23 的并发标记阶段会启用写屏障。该版本的实现采用混合写屏障策略：对指针写入，屏障会对旧指针和新指针进行相应的标记处理；新分配的对象在标记阶段会直接标黑。
    - 插入写屏障： 关注新建立的引用，防止新指向的对象漏标。
    - 删除写屏障： 关注被覆盖的旧引用，避免原本可达的对象因引用关系变化而漏标





#### GC过程

>  并发标记清扫 GC

1. GC 触发：

   > 堆分配增长、达到目标，或显式触发等
   >
   > GOMEMLIMIT？
   >
   > GOGC？

2. Sweep Termination：清扫终止

   > 终止上一轮未完成的清扫

3. Mark Setup：标记准备

   > 短暂 STW，准备标记阶段并启用必要的写屏障

4. Concurrent Mark：并发标记

   > 扫描 GC Roots 和对象引用，识别可达对象

5. Mark Termination：标记终止

   > 短暂 STW，完成标记阶段的收尾工作

6. Sweep：清除

   > 回收未标记对象占用的空间，供后续分配复用





#### 清理：清理不在使用的内存

> 非移动清理：内存重新交还给 Go 的内存分配

todo



### 栈内存

Go 1.23 默认情况下，goroutine 栈通常由 Go 运行时的内存管理器提供，而不是每创建一个 goroutine 就直接调用操作系统申请内存

#### 概念

- 栈缓存：`mcache.stackcache`

- 栈池：`stackpool`
  - `stackpool` 按栈大小等级维护空闲栈，使用相应的锁保护。它管理的是固定大小栈的空闲链表及其所属 span。

#### 申请

```
创建 G，需要栈
      |
      v
P 关联的 mcache.stackcache
      |
      | 缓存命中：直接取出
      |
      | 缓存未命中
      v
全局 stackpool
      |
      | 有空闲栈：取出一批
      | 没有空闲栈：分配新的 span
      v
返回栈空间
```





#### 扩容

1. 申请大栈
2. 拷贝数据
3. 调整指针
4. 是否小栈



#### 释放

```
G 结束
  |
  v
释放当前栈
  |
  +----> 小栈：进入栈缓存或全局 stackpool
  |
  +----> 大栈：进入 stackLarge 缓存
                 |
                 v
         后续 goroutine 复用
```



#### 栈上的对象组织

todo



### 逃逸

编译期决定，对象的内存使用栈内存，还是堆内存



todo





## channle

### 实现方式

```
type hchan struct {
	qcount ：整数类型，表示管道已经存储的数据量。当qcount等于0时，说明管道没有数据可读，此时读管道会阻塞用户协程。
	dataqsiz ：整数类型，表示管道的容量。当qcount等于dataqsiz时，说明管道已经没有剩余容量了，此时写管道会阻塞用户协程。
	buf：指针类型，指向一个数组，用于存储缓存在管道的数据，数组的容量等于elemsize乘以dataqsiz。
	sendx/ recvx：管道本身维护了一个循环数据buf，sendx指向写索引位置，recvx指向读索引位置。
	recvq/ sendq：阻塞协程队列，recvq存储因读管道而阻塞的协程，sendq存储因写管道而阻塞的协程。
	lock：用于锁定管道。管道用于多协程通信，通常是一个协程读管道，另外一个协程写管道，多个协程并发操作同一个数据时需要加锁。
}
```

### 写入过程

- 如果，有等待读取的协程，则直接发送到给协程

- 如果，有剩余缓冲区，则写入缓冲区

- 否则，进入阻塞，将当前协程加入写入等待队列，让出 CPU

### 锁的用途

Go Channel 使用内部互斥锁保护管道状态。发送和接收在持锁时**检查条件**、**更新缓冲区**或等待队列；条件不满足时，将当前 Goroutine 注册到等待队列，并在挂起提交阶段释放锁；条件满足或 Channel 关闭后，运行时再唤醒对应的 Goroutine。

### 使用中的注意事项

1. 未初始化的管道：

   1. 写数据会导致协程永久性阻塞。

   2. 读数据会导致协程永久性阻塞。

2. 已关闭的管道：

   1. 写数据会导致程序抛panic异常。
   2. 再次关闭会导致程序抛panic异常

3. 使管道关闭之后，也可以正常地从管道读取数据，没有数据时直接返回对应的空值

   > 设计意图，保证写入管道的数据，都可以被读取，避免数据丢失



## timer

- golang使用堆作为底层数据结构，因为此结构，定位最大最小值的时间复杂度是 O(1)，更新某个值的时间复杂度 O（logn)，

- 普通的堆是一棵二叉树，而golang使用的是四叉树。这样树的高度更低，堆增删改查的效率更高

- 定时器中主要的字段如下：

```
type timer struct {
	...
	when   int64, // 定时器触发时间 when，
	period int64, 定时器触发周期period，
	f      func(arg any, seq uintptr, delay int64) // // 定时器执行函数f，
	arg    any,  // 定时器执行时所需要的参数arg
	...
}
```

- 逻辑处理器 P 都包含一个定时器堆（runtime.timer数组)

```
type p stuct {
	...
	timers []*timer
	time0When uint64
	...
}
```

- 调度线程(G0) 每次执行 runtime.schedule，首先检查是否有到期的定时器，如果到期的定时器，则先执行定时器



## netpoll

- netpoll 是 Go runtime 的网络 I/O 轮询机制，它让 goroutine 在等待网络数据时不必阻塞整个 OS 线程，而是挂起 G，让 M/P 去执行其他任务。网络就绪后，runtime 再唤醒对应的 G。从而简化了网络编程
- `netpoll` 和 C 的 `libevent` 在核心目标上非常相似：都利用操作系统的 I/O 多路复用机制（如 Linux `epoll`），管理大量连接，并在 I/O 就绪时通知上层处理。区别在于：Go netpoll 集成了 goroutine 调度器，而 libevent 提供事件循环和回调机制



- Epoll介绍  @../../network/epoll.md




## map

Go 1.23的核心是：一个桶数组，每个桶存放最多 8 个键值对，必要时通过 overflow 连接额外的桶。

### 底层结构

```
type hmap struct {
    count     int            // 元素个数
    flags     uint8          // 状态标志（正在写入、正在扩容等）
    B         uint8          // bucket 数量的对数，bucket 数 = 2^B，
    												 // 存储的是 buckets 这个连续桶数组的长度指数：len(buckets) == 2^B
    noverflow uint16         // 溢出 bucket 的近似数量
    hash0     uint32         // 哈希种子
    buckets   unsafe.Pointer // 指向 bucket 数组
    oldbuckets unsafe.Pointer // 扩容时指向旧 bucket 数组
    nevacuate uintptr        // 扩容进度
    extra     *mapextra
}
type bmap struct {
    tophash [bucketCnt]uint8 // 8 个 key 的哈希高 8 位
    // 后面跟着 8 个 key 和 8 个 value
    // 最后可能跟一个 overflow 指针
}

hmap
├── count       元素数量
├── B           主桶数量的对数
├── hash0       哈希随机种子
├── buckets     当前桶数组
├── oldbuckets  扩容时的旧桶数组
└── nevacuate   搬迁进度

bmap
├── tophash[8]
├── keys[8]
├── values[8]
└── overflow 指针

buckets
│
├── bucket 0 ──> overflow
├── bucket 1
├── bucket 2 ──> overflow ──> overflow
├── bucket 3
└── ...

```


### key的定位方法

1. 通过 key 哈希值**低 B 位**确定主桶
2. 再用**高 8 位**（存在 tophash 里）快速定位
3. 如果定位不到，在通过overflow遍历后续的桶，继续定位
4. 最后才逐个比较完整 key


### 扩容机制

- Go 1.23 有两种扩容：

1. 装载因子过高：每个主桶超过 6.5 个元素。执行翻倍扩容
   简化公式：

$$
  count > 6.5 \times 2^B
$$

2. 溢出桶过多：溢出数大于2^B或者2^15。执行等量扩容
   简化公式：

$$
  noverflow \geq 2^{\min(B,15)}
$$



### 翻倍扩容的搬迁规则

假设旧桶数是 $2^B$，扩容后是 $2^{B+1}$。一个 key 的哈希值会多使用一位来决定新位置：

- 若新增的那一位是 `0`，元素留在编号相同的桶，例如旧桶 `x` 仍去新桶 `x`。
- 若新增的那一位是 `1`，元素迁移到 `x + 2^B`。

因此，一个旧桶中的 8 个元素会被拆分到两个新桶中



### 搬迁过程

如果扩容时立刻把几百万个元素全部重新哈希，可能造成明显停顿。Go 因此采用渐进式搬迁：

1. 触发扩容时，运行时分配新桶数组，并把旧数组保存为 `oldbuckets`。
2. 后续执行 `map` 的写入或删除时，运行时会搬迁一小部分旧桶，通常是一次 1 到 2 个桶。
3. 读操作也可能触发搬迁，以保证能正确找到尚未搬走的元素。
4. `nevacuate` 记录已搬迁到的旧桶位置；全部搬完后，释放旧桶数组，扩容结束。victoriametrics+2

这意味着扩容不是一次性的大停顿，而是被摊销到许多次普通 map 操作中



### 等量扩容

**等量整理（same-size growth）不是重新哈希，也不是扩容桶数量，而是把旧主桶及其溢出桶里的元素，按原有桶编号一对一搬到一套新的等量主桶数组中，从而消除溢出桶链。** 它主要解决“反复插入又删除后，元素不多但溢出桶很多”的内存和查找效率问题

### 搬迁过程

假设旧主桶数量为 $2^B$，等量整理后新主桶数量仍是 $2^B$。

1. **分配新主桶数组**：新数组长度等于旧数组长度，但初始没有溢出桶。
2. **逐桶搬迁**：运行时按旧桶编号依次处理，例如旧桶 `i` 的元素全部迁到新桶 `i`。
3. **不改变 key 的归属**：因为新旧桶数量相同，key 的哈希值取模结果不变，所以不需要根据新增哈希位重新分流。
4. **重新紧凑存放**：旧桶和它的溢出桶链中所有仍存在的键值对，会被重新填入新主桶的 8 个槽位；只有新主桶也满了，才会重新创建溢出桶。
5. **释放旧结构**：旧主桶数组和旧溢出桶不再被引用后，由 GC 回收。



## interface

是一个类型的包装器，内部包括了数据的类型和数据，以及函数表。

所以，接口的实例是一个包装器，而不是指针，其内包含了：类型和指针

### 内存布局

#### 空接口

```golang
type eface struct { // 16 bytes
    _type *_type   				// 类型信息：_type，具体类型的元数据
    data  unsafe.Pointer 	// 数据指针：data，具体值的表示或存储地址
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

例如，

```
int 赋值给了any 接口x
var x any = 100

x的内存布局
┌───────────────────┬───────────────────┐
│ _type             │ data              │
│ int 类型元数据地址  │ 具体值的表示/地址    │
└─────────┬─────────┴─────────┬─────────┘
          │                   │
          ▼                   ▼
      int 类型信息           值 100

两个字段分别解决不同的问题：
- _type：告诉运行时，接口中保存的是哪一种具体类型。
- data：提供这个具体值的表示或访问路径。
```



#### 非空接口

```golang
type iface struct { // 16 bytes
    tab  *itab   					// 接口方法表：接口类型、动态类型及对应的方法入口
    data unsafe.Pointer   // 数据指针：data，具体值的表示或存储地址
}
type itab struct { // 32 bytes
    inter *interfacetype
    _type *_type          // 类型信息：_type，具体类型的元数据
    hash  uint32
    _     [4]byte
    fun   [1]uintptr      // 函数表
}
```



### 接口的赋值

1. 检查方法集
2. 



### 接口的比较







**注意**

1. Go 语言只会在传递参数、返回参数以及变量赋值时，才会对某个类型是否实现接口进行检查
2. 与 C 语言中的 void * 不同，interface{} 类型不是任意类型
3. 指针和值类型都可以实现接口的，但是不能同时实现同一个接口；对象都是通过值传递的
4. 指针可以使用值的接口，但是不可以使用指针的接口

**参考**

1. [4.2 接口](https://draveness.me/golang/docs/part2-foundation/ch04-basic/golang-interface/)
2. [iface 和 eface 的区别是什么](https://qcrao91.gitbook.io/go/interface/iface-he-eface-de-qu-bie-shi-shi-mo)

## slice

slice 的底层数据是数组，slice 是对数组的封装，它描述一个数组的片段
底层数组是可以被多个 slice 同时指向的，因此对一个 slice 的元素进行操作是有可能影响到其他 slice 的。
slice 的复制，只是引用的赋值
Go编译器不允许调用了 append 函数后不使用返回值
切的复制，可以通过向空切片，追加元素来实现

```golang
// runtime/slice.go
type slice struct {
    array unsafe.Pointer // 元素指针
    len   int // 长度 
    cap   int // 容量
}
```

```golang
package main

import "fmt"

func main() {
    slice := []int{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}
    s1 := slice[2:5]
    s2 := s1[2:6:7]

    s2 = append(s2, 100)
    s2 = append(s2, 200)

    s1[2] = 20

    fmt.Println(s1)
    fmt.Println(s2)
    fmt.Println(slice)
}
```

```shell
[2 3 20]
[4 5 6 7 100 200]
[0 1 2 3 20 5 6 7 100 9]
```

扩容

当原 slice 容量小于 1024 的时候，新 slice 容量变成原来的 2 倍；
原 slice 容量超过 1024，新 slice 容量变成原来的1.25倍

todo: 不准确

## array

与很多语言不同，Go 语言中数组在初始化之后大小就无法改变，存储元素类型相同、但是大小不同的数组类型在 Go 语言看来也是完全不同的，只有两个条件都相同才是同一个类型。

另一种是使用 [...]T 声明数组，

```golang
arr1 := [3]int{1, 2, 3}
arr2 := [...]int{1, 2, 3}
```

## select

select 关键字也能够让 Goroutine 同时等待多个 Channel 的可读或者可写，在多个文件或者 Channel 发生状态改变之前，select 会一直阻塞当前线程或者 Goroutine。

特性

- select 能在 Channel 上进行非阻塞的收发操作；
- select 在遇到多个 Channel 同时响应时会随机挑选 case 执行；
- 当存在可以收发的 Channel 时，直接处理该 Channel 对应的 case；
- 当不存在可以收发的 Channel 是，执行 default 中的语句；

注意

- select 重的些 case 中的表达式必须都是 Channel 的收发操作
- 如果我们按照顺序依次判断，那么后面的条件永远都会得不到执行，而随机的引入就是为了避免饥饿问题的发生
- 空的 select 语句（没有case）会直接阻塞当前的 Goroutine，导致 Goroutine 进入无法被唤醒的永久休眠状态。

```goalng
type scase struct {
    c           *hchan
    elem        unsafe.Pointer
    kind        uint16
    pc          uintptr
    releasetime int64
}
```

todo:

## defer

Go 语言的 defer 会在当前函数返回之前执行传入的函数

特性

- 向 defer 关键字传入的函数会在函数返回之前运行

原理

```golang
type _defer struct {
    siz     int32 //siz 是参数和结果的内存大小；
    started bool
    sp      uintptr // sp 和 pc 分别代表栈指针和调用方的程序计数器
    pc      uintptr
    fn      *funcval
    _panic  *_panic // 是触发延迟调用的结构体，可能为空；
    link    *_defer
}
```

runtime._defer 结构体是延迟调用链表上的一个元素，所有的结构体都会通过 link 字段串联成链表。

todo:






## sync.Map

采用读写分离的 map，适用于读多、写少的场景。读写 map 的值是指针。

- sync.Map 的结构

  - read：只读map，value 是 entry 的指针。通过原子操作保护，

  - dirty：可更新 map，value 是 entry 的指针。由互斥锁保护、可以增删元素的 map。

  - entry：真正存放键对应的值，并通过原子指针管理其状态。

```golang
// 抽象结构
Map
 ├── mu       保护 dirty 和元数据
 ├── read
 │    └── readOnly
 │         ├── m ──────┐
 │         └── amended │
 ├── dirty             │
 └── misses            │
                       ▼
                 entry
                   └── p → 实际 value

// 实际结构
type Map struct {
    mu     Mutex  			// 保护 dirty 及相关元数据的修改
    read   atomic.Pointer[readOnly] // 原子读取和发布只读快照，负责快速读取。
    dirty  map[any]*entry 	// 保存有效 entry，包括尚未进入 read 的新键；访问需要锁保护，负责新增键及可变映射管理。
    misses int 				// 统计读取进入锁保护慢路径的次数，达到阈值后触发晋升，决定何时将 dirty 晋升为 read。
}

type readOnly struct {
    m       map[any]*entry
    amended bool 			// true 表示 dirty 中存在 read.m 尚未包含的键，相当于缓存失效
}

type entry struct {
    p atomic.Pointer[any] 	// 负责值的原子更新与删除状态。原子指针，指向实际存储的值；也用于表示逻辑删除状态
}

```

### 基本操作

- 增：加锁写入 `dirty`，等待晋升到 `read`
  1. 如果 `dirty == nil`，需要根据 `read` 构建新的 `dirty`。
  2. 如果 `dirty` 已存在，直接向其中插入新键。
  3. 插入时设置 `read.amended = true`，表示 `dirty` 中有尚未进入 `read` 的键。

- 删：通过原子操作将 `entry` 标记为逻辑删除，不直接修改 `read`

- 改：通过原子操作更新 `entry` 中的值，通常不需要加锁。

- 查：优先查 `read`，必要时加锁查 `dirty`



### 何时更新 read

- 情况一：misses >= len(dirty)
  当读取操作因为 read 未命中而进入加锁慢路径时，会增加 misses。达到阈值后，将 dirty 晋升为新的 read。

- 情况二：调用 Range
  如果 read.amended == true，Range 会在加锁后将 dirty 直接晋升为 read，然后遍历新的 read。

### 如何更新 read

1. `read` 指向原来的 `dirty`。
2. `dirty` 置为 `nil`。
3. `misses` 重置为 `0`。
4. 新增的 key 后续也能通过 `read` 直接查询

### 何时读取 dirty？

read中找不到 key，并且 read.amended==true


### 为什么使用atomic.Pointer

让多个 goroutine 能够无锁地读取 read 快照，同时安全地更新 entry 中的值，并保证并发操作的可见性。



## sync.Pool

`sync.Pool` 是一个与 GC 生命周期绑定的临时对象复用池。它通过 P 本地缓存、跨 P 共享缓存和 GC 清理机制，减少高频对象的重复分配及垃圾回收压力。



todo







## todo

小对象如何分配

一个对象是否可以占用两个 slot

三色标记的基本原理。

根对象指的是什么

扫码的对象，具体来说是指 slot 吗

写屏障是如何实现的。

GOGC 含义是什么，GO
