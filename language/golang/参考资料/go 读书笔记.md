# Go 底层原理和工程化实践

## 第2章　Go语言并发模型

### 2.1 GMP调度模型

2.1.2 GMP调度模型概述

1. GM模型中，多个线程 (M) 从全局队列中（有锁）获可运行的 G。调度效率低，数据的本地化程度低
2. GMP模型中， 每个线程（M）绑定一个逻辑处理器(P)，优先从 P 的本地队列（无锁）中获取 G，调度效率高，数据的本地化程度高
3. 协程的栈内存实际来自堆内存，栈内存寄存器（RSP栈顶，RBP 栈底）指向栈内存，栈的切换同样也是修改栈寄存器


2.1.3 深入理解GMP调度模型

每一个线程M都有一个调度协程g0，g0协程的主函数是runtime.schedule，该函数实现了协程调度功能。

每一个协程都有一个协程栈，这个栈不是操作系统维护的，而是Go语言在运行时堆申请的一块内存。

线程M必须绑定逻辑处理器P才能调度协程G，每一个逻辑处理器P都有一个本地可运行协程队列


GMP 各模块的关键字段定义如下：

```go
// 线程描述
type m struct {
	g0		*g 			// 调度协程，执行调度逻辑（runtime.schedule），
	curg 	*g 			// 当地正在调度执行的 g
	p 		puintptr 	// 当前绑定的逻辑处理器 P
}

// 协程描述
type g struct {
	goid 	int64 		  // 协程 Id
	stack 	stack 		// 协程栈，包含两个指针类型的字段，分别指向协程栈的栈顶以及栈
	m 		*m 			// 当前绑定的 线程M
	sched 	gobuf		// 协程上线文，栈寄存器(RSP,RBP)，指令寄存器(PS)
}

// 逻辑处理器 P
type p struct {
	status 	uint32			// 状态,  包括：_Pidel（未绑定 M）,_Prunning（已绑定 M）,_Psyscall（遇到系统调用）,_Pgcstop（被GC 暂停）,_Pdead（废弃的）
	m 		muintptr		// 当前绑定的线程 M
	runq 	[256]guintptr	// 可运行的协程队列
}
```


### 2.2 协程管理

2.2.2 协程创建

创建协程的逻辑都会切换到线程栈执行，协程创建完成后，再切换到原来的协程栈继续执行。我们可以看一下函数runtime.newproc

2.2.3 协程切换

运行中的协程，遇到以下阻塞情况，会触发协程调度，即，暂定当前协程，然偶执行后续行程

1. 锁阻塞：与其他协程抢锁失败，等待对方释放所
2. 管道读写阻塞：管道无数据不可读，或者无剩余空间不可写，就绪
3. 网络 I/O 阻塞：异步 IO 未完成
4. 休眠：协程执行 sleep 函数休眠


协程的切换是通过将协程的上下文中同步到寄存器中实现的。包括栈寄存器BP、SP，指令寄存器PC。最后，通过JMP指令，跳转到将要执行的协程

切换过程如下：
1. M 从 G 的栈切到 G0 的栈
2. M 执行 G0 上的调度函数runtime.schedule
3. 执行新的 G



### 2.3 调度器

2.3.1 调度器实现原理

获取 G 的方式有三种：
1. 一般情况下只需要从当前绑定的逻辑处理器 P 的可运行协程队列(runq)获取协程。
2. 一定条件下也会从全局可运行协程队列获取协程
3. 还会从其他逻辑处理器 P 的本地可运行协程队列去“偷“

调度步骤（即runtime.schedule的实现）：
1. 检查并执行已到期的定时器
2. 每 61 次从全局队列中获一次G
3. 从本地队列中获取G
4. 从全局队列中获取G
5. 从 netpoll 中获取数就绪的 G（已解阻塞）
6. 从其他 P 中偷取G


2.3.2 时间片调度

系统监控线程（runtime.sysmon）周期性 (20us~10ms) 遍历一次所有 P（allp），如果 P 上执行的 G 超过 10ms，则通过信号通知 G 让出 CPU。

通知的方法如下：
1. 修改栈帧的保护字段：stackguard0 = stackPreempt
2. 对线程 M 发送信号：SIGURG

2.3.3 基于协作的抢占式调度

G 的函数调用过程中，通过编译器插入的代码，通过栈保护字段stackguard0判断，是否需要抢占，如果需要，则让出 CPU，然后移入全局队列

2.3.4 基于信号的抢占式调度

M 线程收到信号：SIGURG，中断当前 G 的执行，切换到信号处理函数，该函数会让出 CPU。然后



## 第三章 调度器触发时间

锁阻塞，抢锁导致阻塞
管道阻塞，读写未就绪的管道导致阻塞
休眠阻塞，执行 sleep 导致阻塞
I/O 阻塞，异步 I/O 未就绪导致阻塞
系统调用阻塞，执行系统调用导致阻塞

### 3.1 网络 I/O

网络 I/O 采用异步模式。linux 上使用 epoll，windows 上使用 IOCP 管理异步 IO。

读写操作，遇到不可读(无数据)，不可写的时候(todo)，让出 CPU，即调度器，继续调取其他协程

调度函数runtime.schedule通过runtime.findrunnable函数，调用到netpoll，其中调用epoll_pwait，该函数会返回可读可写的 socket 相关的协程，然后调度函数会将其移动到全局队列中

系统监控线程 runtime.sysmon，周期(20us~10ms)调用netpoll，保障socket 的异步状态检查都能被执行到

I/O 的超时通过socket 上下文中的定时器来实现，如果读写超时，则执行runtime.netpolldeadlineimpl，将 socket 相关的协程恢复到全队调度队列中（todo），该协程恢复执行后，检查到已经超时，则返回相应错误码

### 3.2 管道

- 管道的使用的注意事项

1. 向未初始化的管道写数据会导致协程永久性阻塞。
2. 从未初始化的管道读数据也会导致协程的永久性阻塞。
3. 已关闭的管道写数据会导致程序抛panic异常。
4. 已关闭管道，再次关闭管道也会导致程序抛panic异常
5. 使管道关闭之后，也可以正常地从管道读取数据，没有数据时直接返回对应的空值


- 管道的实现

type hchan struct {
	qcount ：整数类型，表示管道已经存储的数据量。当qcount等于0时，说明管道没有数据可读，此时读管道会阻塞用户协程。
	dataqsiz ：整数类型，表示管道的容量。当qcount等于dataqsiz时，说明管道已经没有剩余容量了，此时写管道会阻塞用户协程。
	buf：指针类型，指向一个数组，用于存储缓存在管道的数据，数组的容量等于elemsize乘以dataqsiz。
	sendx/ recvx：管道本身维护了一个循环数据buf，sendx指向写索引位置，recvx指向读索引位置。
	recvq/ sendq：阻塞协程队列，recvq存储因读管道而阻塞的协程，sendq存储因写管道而阻塞的协程。
	lock：用于锁定管道。管道用于多协程通信，通常是一个协程读管道，另外一个协程写管道，多个协程并发操作同一个数据时需要加锁。
}

- 管道写入过程
1. 如果，有等待读取的协程，则直接发送到给协程
2. 如果，有剩余缓冲区，则写入缓冲区
3. 否则，进入阻塞，将当前协程加入写入等待队列，让出 CPU


### 3.3 定时器

golang使用堆作为底层数据结构，因为此结构，定位最大最小值的时间复杂度是 O(1)，更新某个值的时间复杂度 O（logn)，

普通的堆是一棵二叉树，而golang使用的是四叉树。这样树的高度更低，堆增删改查的效率更高

定时器中主要的字段如下：

type timer struct {
	...
	when   int64, // 定时器触发时间 when，
	period int64, 定时器触发周期period，
	f      func(arg any, seq uintptr, delay int64) // // 定时器执行函数f，
	arg    any,  // 定时器执行时所需要的参数arg
	...
}


逻辑处理器 P 都包含一个定时器堆（runtime.timer数组)

type p stuct {
	...
	timers []*timer
	time0When uint64
	...
}
调度线程(G0) 每次执行 runtime.schedule，首先检查是否有到期的定时器，如果到期的定时器，则先执行定时器

todo，如果保证定时器的精度可以到 0.1ms ~ 10ms（todo：go1.18 中是 10us）

### 3.4 系统调用

1. 当协程执行系统调用时候，调度器标记 P 进入系统调用状态
2. 系统监控线程周期性检查，P 的系统调用状态是否超时（大于检查周期）
3. 如果大于，则解除 P 与 M 的绑定。创建并绑定新 M



## 第5章 GC原理、调度与调优 108

### 5.1 内存管理 108

5.1.1 如何设计动态内存分配器 108

Go语言自己实现的内存分配器一次向操作系统申请一块大内存（如64MB）

第一种思路是，当程序申请内存时，多分配几个字节用于维护内存块状态以及内存块大小，比如，8字节内存来维护当前内存块的基本属性。其中3比特存储当前内存块状态，61比特存储当前内存块大小。

第二种思路是，单独维护一份二进制位数据，使用1比特存储每一个8字节内存的分配状态，其中1表示内存已分配，0表示内存空闲

5.1.2 Go语言内存分配器 110

- Page（页）：系统内存管理的最小单元，大小：8KB。

- mspan（简称 span）：运行时内存分配的最小单元，预定义了63种固定大小（8B～32768B），由 N 个连续页 组成的内存块


```go
type mspan struct {
	// 该mspan管理的关联的页数，每页8KB
	npages    uintptr // number of pages in span
	// 记录内存块分配状态的位
	allocBits  *gcBits
	// 该mspan负责分配的内存块大小
	elemsize    uintptr       // computed from sizeclass or from npages
}
```

- 内存分配的主要逻辑：

1. 根据申请内存的大小，找到合适的 mspan
2. 在mspn 的比特位allocBits中，查找满足条件的连续0比特位。
3. 根据最后0位的索引位置（可以计算出对应内存在mspan的偏移量），以及mspan的首地址，计算返回的内存首地址。


5.1.3 Go语言内存管理 111

- 本地 mspan

```go
type p struct {
	mcache 		*mcache
	pcache      pageCache // 缓存64个空闲页，每页8KB，也就是说可以向该对象申请新的mspa
}

type mcache struct {
	// 共个136，其中 67 个用于包含指针，67 个用于不包含指针，2 个用于大块内存（大于32768字节）
	alloc [numSpanClasses]*mspan 
}

type pageCache struct {
 	base  uintptr // base address of the chunk
 	cache uint64  // 64-bit bitmap representing free pages (1 means free)
 	scav  uint64  // 64-bit bitmap representing scavenged pages (1 means scavenged)
}
```

-  全局 mspan
```go
var mheap_ mheap

type mheap struct {
    // 共个136，其中 67 个用于包含指针，67 个用于不包含指针，2 个用于大块内存（大于32768字节）
    central [numSpanClasses] struct {
        mcentral mcentral
    }
}

type mcentral struct {
    partial   [2]spanSet   // 有空闲内存（mspan）
    full      [2]spanSet   // 无空闲内存（mspan）
}
```

- mspan 的分配过程

1. 从全局mspan(mheap.mcentral)中分配

1). 加锁（由于多个协程可能会并发地访问全局mcentral）
2). 从partial查找已被清理的mspan，如果有，返回该mspan。
3). 从partial查找未被清理的mspan，如果有，则执行垃圾回收清理逻辑，清理之后返回该mspan。
4). 从full查找未被清理的mspan，如果有，则执行垃圾回收清理逻辑，清理之后如果有空闲内存则返回该mspan。

2. 从mcentral查找mspan是有次数限制的，默认查找100次之后如果还没有可用的mspan，则从对象pageCache中申请新的mspan

3. 如果逻辑处理器P的页缓存pageCache也没有足够的空闲页？那就通过页分配器pageAlloc分配页缓存，

4. 如果页分配器pageAlloc页也没有足够的内存，则最终通过线性分配器linearAlloc向操作申请64MB内存，这一整块内存也被称为heapArena。


5.1.4 内存逃逸 113

编译期决定，对象的内存使用栈内存，还是堆内存




### 5.2 三色标记与写屏障 114

5.2.1 三色标记 115

1. 将所有对象，统一标成灰色：标为黑色(1)，移入灰色集合
2. 从灰色集合中逐个取出灰色对象，找到到其中的指针，并标记对应的对象为黑色

如是识别指针

Go语言采用比特位维护这些信息，每一个比特表示对应8字节内存是否存储指针，这些信息维护在heapArena对象。

Go语言使用2比特，分别表示每一个8字节内存是否存储指针，以及后续字节是否需要继续扫描，这样的话针对上述情况就只需要扫描一次了


如何快速回收所有的白色对象呢？

理论上只需要将mspan.gcmarkBits赋值给mspan.allocBit就可以了。

如何识别栈内存？

Go语言专门定义了一个字段(mspan.state)用于判断当前mspan管理的内存是堆内存还是栈内存

5.2.2 写屏障 117

### 5.3 标记与清理 119
5.3.1 垃圾回收执行阶段 120
5.3.2 经典的stopTheWorld 123
5.3.3 辅助标记 125
5.3.4 内存清理 127

### 5.4 GC调度与GC调优 128
5.4.1 GC触发时机 128
5.4.2 GC协程调度模式 131
5.4.3 缓存框架bigcache中的GC调优 133
5.5 本章小结 135

