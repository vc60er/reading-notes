# Goroutine 调度模型（GMP）深度讲义

> 本文以教学讲义的标准撰写，逐层递进。每一章先明确"要回答什么问题"，再展开机制与设计理由，最后附要点小结。涉及源码处标注文件与关键函数名，便于自行查阅验证。

---

## 目录

1. [引子：为什么需要调度器](#1-引子为什么需要调度器)
2. [历史演进：从 GM 到 GMP](#2-历史演进从-gm-到-gmp)
3. [GMP 三要素深度解剖](#3-gmp-三要素深度解剖)
4. [调度循环](#4-调度循环)
5. [阻塞处理全景](#5-阻塞处理全景)
6. [Netpoller 深度解析](#6-netpoller-深度解析)
7. [sysmon 监控线程](#7-sysmon-监控线程)
8. [抢占式调度的演进](#8-抢占式调度的演进)
9. [关键场景全程演练](#9-关键场景全程演练)
10. [设计哲学与总结](#10-设计哲学与总结)
    [参考资料](#参考资料)

---

## 1. 引子：为什么需要调度器

> **本节回答的问题：** 操作系统已经有线程调度器了，Go 为什么还要自己再做一层？协程到底省了什么？M:N 调度模型解决了什么问题？

### 1.1 线程模型的三种形态

用户态并发任务最终必须映射到内核线程（OS thread）上执行。按映射关系的不同，有三种基本模型：

| 模型      | 映射关系               | 代表实现                                    | 优点                            | 缺点                               |
| ------- | ------------------ | --------------------------------------- | ----------------------------- | -------------------------------- |
| **1:1** | 一个用户线程对应一个内核线程     | Linux pthread, Java Thread (早期)         | 真并行，一个线程阻塞不影响其他               | 创建/切换成本高（内核态），栈固定大（~1MB），数量受限于内核 |
| **N:1** | 多个用户线程复用同一个内核线程    | Python gevent (greenlet), Lua coroutine | 切换极快（纯用户态），内存占用极小             | 无法利用多核，一个阻塞全部阻塞                  |
| **M:N** | M 个用户线程映射到 N 个内核线程 | **Go goroutine**, Erlang process        | 兼具两者优势：轻量 + 多核并行；阻塞时只阻塞当前内核线程 | 实现复杂度高——需要一套用户态调度器               |

**Go 选择了 M:N 模型**，这就是 GMP 调度器存在的根本原因：它需要做一件操作系统线程调度器不会做的事——在用户态将大量 goroutine（M 个）高效地映射到较少的 OS 线程（N 个）上。

### 1.2 协程到底省了什么

> **设计理由：** 这两个量化数据是 Go 选择 M:N 模型的核心经济学依据。如果协程的栈和切换成本跟线程差不多，就没必要在用户态再做一层调度。

**栈内存：**

goroutine 的栈初始 2KB（`_StackMin`），支持按需扩容和缩容。OS 线程栈大小在创建时固定（Linux 默认 8MB）。[2][3]

|           | 初始栈      | 最大栈       | 增长策略                      | 谁管理                            |
| --------- | -------- | --------- | ------------------------- | ------------------------------ |
| OS 线程     | ~8 MB    | = 初始值（固定） | 无——溢出即 SIGSEGV            | 内核（预留虚拟空间，懒分配物理页）              |
| Goroutine | **2 KB** | 1 GB      | 翻倍增长；GC 时若使用 < 1/4 则收缩为一半 | Go runtime（`runtime/stack.go`） |



**上下文切换成本：**

线程切换需经过内核，goroutine 切换全程在用户态

|              | 耗时量级              | 涉及什么                                         |
| ------------ | ----------------- | -------------------------------------------- |
| 线程切换         | ~1-10 μs          | 内核态陷入、保存/恢复完整寄存器组、TLB flush、可能的 cache miss   |
| Goroutine 切换 | ~0.2 μs（约 200 ns） | 用户态，仅保存/恢复编译器确定在用的寄存器（通过 `gobuf` 结构），不经过内核** |



### 1.3 Go 的答案：不共享内存来通信，而是通过通信来共享内存

这句著名口号不仅仅是编程风格建议——它直接影响了调度器的设计方向。如果 goroutine 之间主要通过 channel 通信（而非共享内存加锁），那么调度器在遇到阻塞时就可以做出更激进的决策（如 work stealing、hand off），因为 channel 的阻塞/唤醒是可预测、可管理的，而共享内存的锁竞争会引入调度器无法感知的阻塞。

### 本节要点

- Go 选 M:N 是为了兼得"轻量协程"和"多核并行"
- 协程轻量的物质基础：2KB 初始栈 + 按需增长/收缩（compiler 插桩 → `copystack` 翻倍扩 + GC 触发缩）。OS 线程做不到——内核没有编译器提供的栈指针映射（详见 `gpm-scheduling-qa.md` Q2）
- 用户态切换比同进程线程切换快 ~20 倍（~50-500ns vs 1-10μs）——根本原因：零内核参与、仅保存 3 个寄存器、无地址空间操作（详见 `gpm-scheduling-qa.md` Q1）
- 调度器的设计目标：将百万 goroutine 映射到 GOMAXPROCS 个 OS 线程，同时保持公平性和低延迟

---

## 2. 历史演进：从 GM 到 GMP

> **本节回答的问题：** GMP 不是从天上掉下来的——Go 1.0 的 GM 模型是什么样的？它有什么致命缺陷？引入 P 到底解决了什么？

### 2.1 Go 1.0：GM 模型

在 Go 1.0 时代（2012 年 3 月发布），调度器只有两个角色：

- **G**（Goroutine）：用户态任务
- **M**（Machine）：OS 线程

调度方式异常简单——一个全局 goroutine 队列，用一把**全局互斥锁**（`Sched.Lock`）保护。所有 M 都从同一把锁下抢 G。

```
┌──────────────────────┐
│  Global Run Queue     │  ← 一把 Sched.Lock 保护
│  [G] [G] [G] [G] ... │
└──────┬───────┬───────┘
       │       │
    ┌──▼──┐ ┌──▼──┐
    │ M1  │ │ M2  │  ... 所有 M 争抢同一把锁
    └─────┘ └─────┘
```

### 2.2 GM 模型的四大缺陷

> **参考资料：** 这些缺陷的原始分析出自 Dmitry Vyukov 2012 年的 *Scalable Go Scheduler Design Doc* [4]，他当时在 Vtocc 服务器上观测到 8 核机器只能跑到 70% 的 CPU 利用率，其中 14% 的时间浪费在 `runtime.futex()` 上。

**缺陷一：全局互斥锁争用（Scalability Killer）**

每次创建、完成、重新调度 goroutine 都需要获取 `Sched.Lock`。随着核心数增加，锁争用急剧恶化。在最坏情况下（所有核心都在做 goroutine 操作），调度器本身变成了瓶颈——CPU 越多，浪费在锁等待上的比例越高。

**缺陷二：Goroutine 在 M 之间频繁传递（`G.nextg`）**

当一个 M 产生新的可运行 G 时，它往往不是自己执行，而是通过 `G.nextg` 传递给另一个空闲 M。这引入了额外的延迟——G 从一个 M"旅行"到另一个 M 的过程中：

- 两个 M 需要交互
- 目标 M 的 cache 是冷的（没有这个 G 的上下文）
- 在高速核心间造成不必要的跨核心通信

**缺陷三：Per-M 内存缓存导致资源浪费（`M.mcache`）**

每个 M 都持有一个独立的内存分配缓存（`mcache`，可高达 2 MB）。问题在于：

- 一个 M 被系统调用阻塞时，它的 mcache 也跟着闲置，无法被其他活跃的 M 利用
- 一个服务器可能同时有 100 个 M 被阻塞在系统调用上，却只有几个 M 在活跃执行——大量 mcache 白白浪费
- 活跃 M / 空闲 M 的比例可能高达 1:100

**缺陷四：系统调用导致的剧烈线程阻塞/唤醒**

当 M 执行系统调用被阻塞后，调度器必须创建新的 M 来接替工作；系统调用返回后，原来的 M 又被唤醒。这种频繁的创建/销毁/阻塞/唤醒造成了显著的性能抖动。

### 2.3 Go 1.1：引入 P，一步到位

> **参考资料：** Vyukov 的设计文档 *Scalable Go Scheduler Design Doc* [4] 的核心思想一句话：**将"需要执行什么"（G）、"在哪里执行"（M）、"执行所需的资源"（P）三者解耦。**

Vyukov 的解决方案不是修补 GM，而是重新定义模型——引入 **P（Processor）**。这不是简单的"多加一层"，而是一次彻底的关注点分离：

| 实体    | 语义      | 数量                               | 核心职责                         |
| ----- | ------- | -------------------------------- | ---------------------------- |
| **G** | 要执行什么   | 无上限                              | 持有 goroutine 的执行上下文（栈、PC、状态） |
| **M** | 在哪里执行   | 无上限（受 OS 限制）                     | OS 线程，实际占用 CPU 执行指令          |
| **P** | 执行所需的资源 | **固定 = GOMAXPROCS**（通常 = CPU 核数） | 持有本地运行队列、mcache 等执行资源        |

关键设计决策：

- **P 持有 mcache**（而不是 M），这样 M 阻塞时 mcache 可以立即转给下一个 M
- **P 持有本地运行队列**（lock-free 环形缓冲区），消除了全局锁
- M 必须**先绑定 P**才能执行 Go 代码——P 是执行 Go 代码的"许可证"
- P 的数量固定为 `GOMAXPROCS`——这直接决定了**真正并行的 goroutine 数量上限**

```
Go 1.1 GMP 模型：

    Gs:  [G] [G] [G] [G] [G] [G] [G] [G] ...
          │        │
    Ps: ┌─▼──┐  ┌──▼──────┐
        │ P0 │  │   P1    │  ← 各持有本地 runq
        │ mcache│  │ mcache │
        └─┬──┘  └──┬──────┘
          │        │
    Ms: ┌─▼──┐  ┌──▼──┐
        │ M0 │  │ M1 │  ← 必须绑定 P，才能执行 Go 代码
        └────┘  └────┘
```

### 2.4 为什么不是"优化 GM"而是"引入 P"

> **设计理由：** 这是一个常见的疑问——为什么不直接在 GM 基础上优化（比如去掉全局锁、引入 per-M 队列），而要引入一个新概念？

**答案在于"执行资源"需要和"执行者"解耦。** 如果不引入 P：

- 要让 mcache 不被阻塞的 M 浪费，就需要把 mcache 从 M 上剥离出来放到某个地方——这就是 P
- 要让运行队列多核扩展，就需要把队列从全局拆到各个执行单元——M 不合适，因为 M 数量不固定（会被阻塞），需要一个固定数量的持有者——这就是 P
- 要支持 work stealing，就需要一个固定大小的数组来遍历偷取目标——M 数量不固定，P 数量固定

P 就是这三个需求的自然交点。它不是被"发明"出来的，而是被"提取"出来的——从 M 身上把不该属于它的职责剥离，形成了一个独立概念。

> **源码索引：** Go 1.0 到 Go 1.1 的 diff 可以在 Go 官方 git 仓库 [14] 查看：`github.com/golang/go` 的 `go1.0` 到 `go1.1` tag 之间。关键改动在 `src/pkg/runtime/proc.c`。

### 本节要点

- GM 的致命缺陷：全局锁、G 传递、mcache 浪费、线程抖动
- P 的本质是**从 M 剥离执行资源**，使资源可以在 M 阻塞时立即复用
- P 数量固定这一设计决策，为 work stealing、负载均衡等后续机制奠定了前提

---

## 3. GMP 三要素深度解剖

> **本节回答的问题：** G、M、P 的 Go 源码结构体里到底存了什么？每个字段为什么存在？goroutine 的一生经历哪些状态？

### 3.1 G（Goroutine）—— 执行单元

> **源码索引：** 结构体定义在 `runtime/runtime2.go`，类型为 `type g struct` [2]。创建流程在 `runtime/proc.go` 的 `newproc1()` 函数 [1]。Kennedy（Ardan Labs）的 *Scheduling In Go* 系列 [8] 对 G 的字段语义与实际调度行为的对应关系有详细工程解读。

**关键字段分组（按职责划分）：**

| 字段组       | 关键字段                                        | 职责                                                                                                           |
| --------- | ------------------------------------------- | ------------------------------------------------------------------------------------------------------------ |
| **执行上下文** | `sched` (gobuf)                             | 保存 goroutine 被切出时的 sp、pc、bp、返回地址等。切换时从这里恢复 CPU 状态，gobuf 是 goroutine 能被"暂停再恢复"的物理基础                           |
| **栈信息**   | `stack` (stack{lo, hi})、`stackguard0`       | `stack` 记录当前栈的起止地址；`stackguard0` 是栈溢出警戒线——每次函数调用前编译器插入的检查就比对这个值。当 `stackguard0 = stackPreempt` 时，触发的是抢占而非栈扩容 |
| **调度状态**  | `atomicstatus`、`m`、`preempt`、`paniconfault` | `atomicstatus` 以原子方式维护当前状态（见下方状态机）；`m` 指向当前绑定的 M                                                             |
| **阻塞信息**  | `waitreason`                                | 记录阻塞原因（channel send/recv、mutex、I/O、sleep 等），用于调试和 profiling                                                  |
| **创建信息**  | `goid`、`gopc`                               | `goid` 是 goroutine 的唯一 ID；`gopc` 记录 `go` 语句的调用地址，用于 profiling 定位创建点                                          |
| **回收复用**  | `gfree` 链表相关字段                              | goroutine 退出后，其 `g` 结构体不是还给堆分配器，而是放入 P 或全局的 `gfree` 空闲链表，下次 `newproc1` 优先从链表取用。**这避免了频繁的堆分配/GC 压力**          |

> **设计理由：** 所有 `g`、`m`、`p` 对象都是堆分配的，但**永不释放**——它们只在空闲链表和活跃状态之间流转。这是刻意为之：保持这些对象的类型稳定性（type-stable），使调度器核心路径不需要写屏障（write barrier），从而可以在 GC 的关键区安全运行。[3]

**Goroutine 状态机：**

```
                    ┌──────────────────────────────┐
                    │                              │
   newproc1        │     goready / channel唤醒     │
      │            │         (放回 runq)            │
      ▼            │                              │
  _Gidle ──► _Grunnable ──► _Grunning ──► _Gdead
               ▲    ▲            │  │           ▲
               │    │            │  │           │
               │    └────────────┘  │           │
               │    (从 syscall     │           │
               │     恢复后重入     │           │
               │     runq)         │           │
               │                   │           │
               │              ┌────┘           │
               │              │                │
               │         _Gsyscall ────────────┘
               │         (进入系统调用)    (正常退出)
               │
               └── _Gwaiting
                  (gopark: channel/mutex/sleep等阻塞)

        _Grunning ──► _Gcopystack ──► _Grunning
                   (栈扩容/缩容时  (copystack 完成)
                   短暂经过此态)
```

> 注：GC 扫描栈时会在上述基础状态上叠加 `_Gscan = 0x1000` 掩码，形成 `_Gscanrunnable`、`_Gscanrunning`、`_Gscansyscall`、`_Gscanwaiting` 等组合状态。这不是独立的状态转换，而是**并发标记期间 GC 对特定 G 的栈进行扫描时的临时锁**——任何试图修改该 G 状态的操作必须等待 `_Gscan` 位清除。

**各状态说明：**

| 状态            | 含义        | G 在哪里                                  | 何时进入                         | 何时离开                                         |
| ------------- | --------- | -------------------------------------- | ---------------------------- | -------------------------------------------- |
| `_Gidle`      | 刚分配，尚未初始化 | gfree 链表                               | `newproc1` 从 gfree 取到 G 的初始态 | 初始化完成 → `_Grunnable`                         |
| `_Grunnable`  | 就绪，等待被调度  | P 的本地队列或全局队列                           | `goready`、`newproc1` 完成      | M 取走 → `_Grunning`                           |
| `_Grunning`   | 正在执行      | 绑定在某个 M+P 上                            | `execute()` 函数调用前            | 正常结束 / gopark / entersyscall / 被抢占           |
| `_Gsyscall`   | 正在执行系统调用  | 绑定在 M 上（可能已失去 P）                       | `entersyscall()`             | `exitsyscall()` → `_Grunnable` 或 `_Grunning` |
| `_Gwaiting`   | 等待中被挂起    | 不在任何运行队列中（在 channel/mutex/timer 等等待队列） | `gopark()`                   | 等待条件满足 → `goready()` → `_Grunnable`          |
| `_Gcopystack` | 栈正在被移动（扩容/缩容） | 不在任何运行队列（短暂） | `newstack()`/`shrinkstack()` 触发 → `copystack()` 执行中 | copystack 完成 → `_Grunning` |
| `_Gdead`      | 已退出       | gfree 空闲链表                             | goroutine 函数返回，`goexit0()`   | 被 `newproc1` 复用 → `_Grunnable`               |
| `_Gpreempted` | 被抢占       | 与 `_Grunnable` 的处理相同，但多了"被强制切出"的语义标记   | 抢占信号处理                       | 下次被调度 → `_Grunning`                          |
| `_Gscan...`   | GC 扫描栈（组合态） | 在基础状态上叠加 `_Gscan=0x1000` 掩码 | GC 并发标记阶段，对被扫描 G 的状态上锁 | 扫描完成 → 掩码清除，回到对应基础状态 |

### 3.2 M（Machine）—— 执行载体

> **源码索引：** 结构体定义在 `runtime/runtime2.go`，类型为 `type m struct`。

**关键字段分组：**

| 字段组      | 关键字段                | 职责                                                                                                                                                    |
| -------- | ------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------- |
| **调度关联** | `g0`、`curg`、`p`     | `g0` 是每个 M 独有的"调度 goroutine"——它有独立的栈（~8KB，比用户 goroutine 大），专门用来执行调度代码（schedule、findrunnable 等），**永远不执行用户代码**。`curg` 指向当前正在执行的用户 goroutine。`p` 指向绑定的 P |
| **线程信息** | `procid`、`thread`   | 操作系统线程 ID                                                                                                                                             |
| **自旋状态** | `spinning`          | 标记 M 是否处于自旋状态——即没有 G 可执行但在主动寻找，而不是睡眠。自旋 M 的数量受限于 `GOMAXPROCS/2`，防止空转浪费 CPU                                                                            |
| **信号处理** | `gsignal`           | 每个 M 还有一个 `gsignal` goroutine，用于处理信号（包括异步抢占的 SIGURG），在信号栈上执行                                                                                          |
| **系统调用** | `locks`、`softfloat` | `locks` 计数器用于阻止抢占——调度器关键区会 `locks++`，只要 `locks > 0`，即使收到抢占信号也不会切出                                                                                     |

**M 的特殊 goroutine：g0 和 gsignal**

> **设计理由：** 为什么调度代码要在 g0 上运行而不是在 curg 上？

普通 goroutine（curg）的栈是**可增长/收缩**的——这意味着它的 SP 随时可能触发栈扩容。而调度器代码：

1. 不能被栈扩容打断——调度器正在决定"接下来运行哪个 G"，栈扩容本身需要调度器配合
2. 需要固定大小的栈以保证确定性
3. 不能被 GC 扫描栈的写屏障干扰

g0 正是为此而生：拥有独立且不可增长的栈，调度逻辑运行其上，与用户代码物理隔离。`mcall` 函数负责从 curg 切换到 g0 栈——这是从用户代码进入调度器的关键跳板。

**M 的生命周期：**

```
newm() ──► newm1() ──► newosproc() (clone 系统调用) ──► mstart() ──► schedule()
                                                             │
                                                    ┌────────┘
                                                    │
                                              [schedule() 循环]
                                                    │
                                                    ├── 找到 G → execute() → gogo()
                                                    ├── 找不到 G → stopm() → 睡眠 (futex)
                                                    └── 被唤醒 → 回到 schedule()
```

Ms 不会轻易被销毁——空闲的 M 通过 `stopm()` 陷入 futex 睡眠，后续被 `startm()` 唤醒复用。只有当长时间空闲且 M 数量过多时才会被回收。

### 3.3 P（Processor）—— 执行资源

> **源码索引：** 结构体定义在 `runtime/runtime2.go`，类型为 `type p struct`。

**关键字段分组：**

| 字段组          | 关键字段                                                          | 职责                                                                                                                                                           |
| ------------ | ------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **运行队列（核心）** | `runq` [256]guintptr、`runqhead`/`runqtail`、`runnext` guintptr | `runq` 是一个 **lock-free 的环形缓冲区**，容量固定 256 个 G。`runqhead`/`runqtail` 分别是消费者/生产者的游标——**本 P 从 head 取，其他 P 从 tail 偷**。`runnext` 是一个特殊的单槽——存放下一个优先执行的 G，优先级高于 runq |
| **内存分配**     | `mcache`                                                      | 每个 P 持有独立的内存分配缓存，消除多核分配内存时的锁争用。M 绑定 P 后使用 P 的 mcache，M 阻塞时 mcache 随 P 转移给下一个 M                                                                               |
| **调度统计**     | `schedtick`、`syscalltick`                                     | `schedtick` 递增于每次调度，决定何时（每 61 次）检查全局队列。`syscalltick` 被 sysmon 用来判断 P 是否在系统调用中停留过久                                                                            |
| **缓存**       | `pcache`、`deferpool`                                          | 与内存分配类似，各类缓存以 P 为粒度，避免跨核共享                                                                                                                                   |
| **状态**       | `status`                                                      | 标记 P 的当前状态：`_Pidle`（空闲，在全局 idle 链表）、`_Prunning`（正在执行 Go 代码）、`_Psyscall`（M 已进入系统调用但 P 尚未被 sysmon 夺走）、`_Pgcstop`（因 GC 暂停）、`_Pdead`（终止）                         |
| **抢占标记**     | `preempt`                                                     | sysmon 设置此标记，schedule 在取下一个 G 之前检查，触发抢占                                                                                                                      |

**runq 的工作方式详解：**

```
runq:  lock-free 环形缓冲区 (容量 256)
         runqhead (消费者，本 P)           runqtail (生产者，本 P)
             │                                  │
             ▼                                  ▼
    ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
    │ G │   │   │   │   │ G │ G │ G │ G │   │  ← 最多 256 个 G
    └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘
                                              ▲
                                        其他 P steal
                                        (从 tail 偷，避免与本 P 竞争)
```

- **本 P 从 head 取（pop），其他 P 从 tail 偷（steal）**——这样本 P 和偷取者操作的是环形缓冲区的不同端，几乎不会产生 CAS 冲突
- **偷取量 = 一半**：`runqsteal()` 的 `runqgrab()` 会取走 `n/2` 个 G（`n = tail - head`），而不是只偷一个。批量偷取减少了 steal 的频率和开销
- **runnext 是特殊通道**：新创建的 G 优先放入 `runnext`。如果 `runnext` 已有值，旧值被挤到 runq 尾部。这是 Vyukov 设计中的一个精妙细节——新创建的 G 往往与当前 G 共享数据（cache 热），尽快执行它有利于局部性

**P 的数量与 GOMAXPROCS：**

> **设计理由：** `GOMAXPROCS` 默认为 CPU 逻辑核数。这不是随意选择——它反映了一个物理约束：**真正同时执行 Go 代码的 M 数量不可能超过 CPU 核心数**。再多设也没有并行度增益，反而增加调度开销。

- 但 `GOMAXPROCS` **不等于**能同时存在的 M 数量——M 在执行系统调用时不被计入（因为它不占 CPU），此时可以有超过 `GOMAXPROCS` 个 M 存在
- I/O 密集型应用可适当调大 `GOMAXPROCS`，计算密集型应用通常用默认值即可
- 在容器化环境中（有 cgroup CPU 限制），Go 1.23+ 的 runtime 能自动感知 cgroup 限制并调整 `GOMAXPROCS`

### 3.4 全局调度器上下文（schedt）

> **源码索引：** `runtime/runtime2.go` 中的 `type schedt struct`，全局唯一实例是 `runtime.proc.go` 中的 `var sched schedt`。

| 关键字段                        | 职责                                                                  |
| --------------------------- | ------------------------------------------------------------------- |
| `runq` (gQueue)、`runqsize`  | 全局运行队列——当 P 的本地队列满（>256）时，多余的 G 会放入此处                               |
| `pidle` (puintptr)、`npidle` | 空闲 P 的单向链表和计数                                                       |
| `midle` (muintptr)、`nmidle` | 空闲 M 的单向链表和计数                                                       |
| `nmspinning`                | 当前正在自旋的 M 数量                                                        |
| `lock` (mutex)              | 全局调度器锁——仅在必须全局同步的操作（如访问全局队列、idle P/M 链表操作）时才加锁，热路径（如本地 runq 的存取）不需要 |

### 本节要点

- G 的核心是 gobuf（保存切换状态）+ atomicstatus（驱动状态机）。gobuf 是 goroutine 能"暂停-恢复"物理基础
- g0 是每个 M 上的"调度栈"——物理隔离用户代码与调度逻辑
- P 的核心是 lock-free 本地队列（runq + runnext + 环形缓冲）——这是消除 GM 全局锁的关键
- 所有 g/m/p 对象永不释放、只在链表中复用——为了类型稳定性、免除调度路径上的写屏障

---

## 4. 调度循环

> **本节回答的问题：** 一个 M 拿到 P 之后，具体怎么找下一个 G？找的优先级是什么？怎么完成从"调度器代码"到"用户代码"的跳转？用户代码执行完后怎么回到调度器？

### 4.1 核心闭环

Go 调度器在每一个 M 上形成一个**永不退出的闭环**：

```
schedule() ──► execute() ──► gogo(asm) ──► [用户代码执行]
    ▲                                            │
    │                          ┌─────────────────┤
    │                          │                 │
    │                    [正常退出]          [阻塞]
    │                          │                 │
    │                      goexit()          gopark()
    │                          │                 │
    │                      goexit1()         park_m()
    │                          │                 │
    │                      goexit0()              │
    │                          │                 │
    └──────────────────────────┴─────────────────┘
```

这个环意味着：

1. 调度器从来没有"返回"到某处——它永远在找 G、执行 G、回到调度器的封闭循环中
2. M 的生命就是在这个环里不断循环，直到程序退出
3. 用户代码看起来是"被调度器调用"了，但实际上是通过 `gogo` 汇编跳转过去的——不是普通函数调用，而是**控制流的彻底接管**

### 4.2 `schedule()` —— 调度器的主循环

> **源码索引：** `runtime/proc.go` 中的 `schedule()` 函数 [1]。这是理解和调试调度行为最重要的单一函数。调度循环的逐步走读亦见 devblogs.sh 的 "The Scheduler" 系列 [6]。

```go
// schedule() 核心逻辑（简化，标注了关键决策点）
func schedule() {
    mp := getg().m

    // 【设计理由】locks++ 阻止抢占：
    // 调度器在执行"选择下一个 G"的关键操作，此时不能被 SIGURG 打断。
    // 如果在此被抢占，可能导致同一个 P 被两个 M 同时操作。
    mp.locks++

top:
    // 【设计理由】检查当前 M 是否绑定了 P：
    // 一个正常的 M 应该持有 P 才能执行 Go 代码。
    // 如果 M 没有 P（例如刚从 syscall 返回但没抢到 P），则 park 自己。
    pp := mp.p.ptr()
    if pp == nil {
        stopm()  // 没有 P 的 M 直接睡眠
        goto top
    }

    // 【设计理由】检查 P 的 preempt 标记：
    // sysmon 可能对当前 P 设置了抢占标记，此时先清除标记再继续。
    if mp.p.ptr().preempt {
        mp.p.ptr().preempt = false
    }

    var gp *g
    var inheritTime bool

    // 第一步：尝试从本地 runq 取（最快路径，lock-free，O(1)）
    if gp, inheritTime = runqget(pp); gp != nil {
        goto found
    }

    // 第二步：完整查找（可能阻塞）
    // findrunnable() 会按全局队列→netpoller→work stealing 的顺序查找。
    // 如果实在找不到可执行的 G，findrunnable() 内部会 stopm() 使 M 进入睡眠。
    gp, inheritTime = findrunnable()

found:
    // 【设计理由】resetspinning：
    // 如果当前 M 之前处于自旋寻找状态，现在找到了工作，置为非自旋。
    resetspinning(mp)

    // 跳入用户代码——execute 永不返回（通过 gogo 汇编跳转）
    execute(gp, inheritTime)

    // 这行代码永远不会被执行到
}
```

### 4.3 `findrunnable()` —— 四级查找优先级

> **源码索引：** `runtime/proc.go` 中的 `findrunnable()` 函数 [1]。这是调度器里最长、分支最多的函数之一——因为它要处理"到哪里找 G"的所有可能路径。四级查找优先级的设计来自 Vyukov 的原始提案 [4]。

查找顺序和优先级：

| 优先级    | 来源          | 操作                                   | 代价                   | 设计理由                  |
| ------ | ----------- | ------------------------------------ | -------------------- | --------------------- |
| ① (最高) | 本地 P 的 runq | `runqget(pp)`                        | O(1), lock-free      | 局部性最好，cache 最热，零同步开销  |
| ②      | 全局 runq     | `globrunqget(pp, max)`               | 需要 `sched.lock`（全局锁） | 保证公平性——防止 G 在局部队列中饿死  |
| ③      | Netpoller   | `netpoll(0)` (non-blocking)          | O(就绪FD数量)            | 网络 I/O 密集场景下的主要 G 来源  |
| ④ (最低) | 其他 P 的 runq | `runqsteal(pp, ...)` (work stealing) | O(P数量)，含 CAS         | 负载均衡的最后手段——"主动发起的再平衡" |

**为什么 1/61 次调度检查全局队列？**

> **设计理由：** 每 61 次调度（`pp.schedtick % 61 == 0`）检查一次全局队列。两个问题：为什么不每次都查？为什么是 61？

- **不每次都查的原因**：查全局队列要获取 `sched.lock`，如果每个 P 每次调度都去抢这把锁，锁争用会让改进退化回 GM 模型。这正是 Vyukov 在设计文档 [4] 中重点避免的问题
- **选 61 的原因**：61 是质数，可以避免多个 P 的检查周期与系统节奏同步——如果所有 P 同时抢全局锁，瞬间争用会很严重。质数步长将各 P 的检查点均匀分散

**从全局队列取多少？**

```go
// globrunqget 的拉取策略（简化）：
n := sched.runqsize / gomaxprocs + 1   // 按 P 数均分
if n > sched.runqsize / 2 {            // 但不超过一半
    n = sched.runqsize / 2
}
```

> **设计理由：** 一次从全局队列取一组 G 放入本地队列，而不是取一个。这样后续多次调度都可以走本地路径（lock-free），摊薄了全局锁的开销。取"不超过一半"是防止一个 P 把全局队列搬空，导致其他 P 只能走 work stealing。

**Work Stealing 的细节：**

```go
// runqsteal 的偷取策略：
// 1. 随机选择起始位置（防止所有 P 从 P0 开始扫，造成前几个 P 被频繁"骚扰"）
// 2. 遍历所有 P，跳过自己、跳过空闲的 P（有 idlepMask 位图快速跳过）
// 3. 从目标 P 的 runq 偷走一半（从 tail 方向，而目标 P 自己从 head 方向取）
// 4. 最多尝试 4 轮，最后一轮还会尝试偷 runnext
```

> **设计理由：** 随机起始位置 + 从 tail 偷 = 最大程度减少偷取者和被偷取者之间的 CAS 重试。偷一半（不是一半）是为了一次性摊薄偷取开销。

**找不到 G 时怎么办？**

如果 findrunnable 遍历了所有来源都找不到 G，它调用 `stopm()`：

1. M 标记为非自旋
2. M 放入全局 `sched.midle` 空闲 M 链表
3. 通过 `notesleep()` → futex 系统调用使 OS 线程进入内核睡眠
4. 后续有新 G 生成或 netpoller 事件时，`wakep()` 或 `startm()` 会通过 futex wake 唤醒它

### 4.4 `execute()` 和 `gogo()` —— 控制权转移

> **源码索引：** `runtime/proc.go` 中的 `execute()` 函数，以及 `runtime/asm_amd64.s` 中的 `gogo()` 汇编实现。

```go
func execute(gp *g, inheritTime bool) {
    mp := getg().m

    // 1. 将当前 M 与 G 绑定
    mp.curg = gp
    gp.m = mp

    // 2. 设置栈溢出警戒线
    //    预留 256 字节的 stackGuardGap，防止函数序言在检查栈之前就溢出
    gp.stackguard0 = gp.stack.hi - stackGuardGap

    // 3. 处理时间片继承（被 steal 的 G 仍算在"受害者"P 的时间配额里）
    if inheritTime {
        mp.p.ptr().schedtick++
    }

    // 4. 汇编跳转！NEVER RETURNS
    gogo(&gp.sched)
}
```

`gogo` 是一个汇编函数，它的核心操作只有三步：

```asm
// runtime/asm_amd64.s (简化逻辑)
TEXT runtime·gogo(SB), NOSPLIT, $0-8
    MOVQ    buf+0(FP), BX       // 获取 gobuf 地址
    MOVQ    gobuf_g(BX), DX     // 1. 加载目标 G
    MOVQ    gobuf_sp(BX), SP    // 2. 切换到目标 G 的栈
    MOVQ    gobuf_pc(BX), BX    // 3. 加载目标程序计数器
    JMP     BX                   // 跳转！(不是 CALL，没有 RET)
```

`JMP` 而非 `CALL` —— 这是关键。如果是 CALL，被调用函数返回后会回到 gogo 的下一行指令，而 JMP 意味着控制权永久转移。那用户代码怎么回到调度器呢？答案在 gobuf 里预设好了：

### 4.5 `goexit0()` —— 为什么 G 执行完能回到调度器

> **源码索引：** `runtime/proc.go` 中的 `goexit0()`，以及 `runtime/asm_amd64.s` 中的 `goexit`。

每个 goroutine 创建时（`newproc1`），它的 `gobuf.pc` 被设为目标函数的地址，而**栈上的返回地址被设为 `runtime.goexit`**：

```go
// newproc1 中的关键代码（简化）
newg.sched.pc = funcPC(goexit) + sys.PCQuantum  // 这是"返回后去哪"
newg.sched.sp = newg.stack.hi - ...              // 栈上预留返回地址
// 在栈上写入 goexit 作为"返回地址"
```

当 goroutine 的入口函数（如你写的 `handler()`）返回时，CPU 从栈上弹出返回地址——也就是 `goexit`。于是：

```
用户函数 return → goexit (asm) → goexit1() → goexit0() → schedule()
```

`goexit0()` 的职责非常明确：

1. 将 G 的状态设为 `_Gdead`
2. 解除 G 与 M 的绑定
3. 将 G 放入 gfree 空闲链表（`gfput`）
4. 调用 `schedule()` —— 回到调度循环

这就是闭环的完整实现。

### 本节要点

- 调度器的核心是一个永不退出的闭环：schedule → execute → gogo → 用户代码 → goexit → schedule
- findrunnable 遵循四级优先级：本地队列 → 全局队列（每 61 次）→ netpoller → work stealing
- gogo 使用 JMP 而非 CALL 做控制权转移——这不是函数调用，是控制流的彻底接管
- goroutine 的返回地址被预设为 goexit，确保用户代码"返回"时天然回到调度器
- inheritTime 机制让被偷的 G 仍算在原 P 的时间配额中，维护跨 P 的公平性

---

## 5. 阻塞处理全景

> **本节回答的问题：** goroutine 阻塞时，底层到底发生了什么？为什么 channel 阻塞和文件 I/O 阻塞的处理方式完全不同？哪些阻塞只挂起 G，哪些连 M（甚至 P）也被牵连？

一个 Go 程序中的阻塞大致分四类，它们对 G/M/P 的影响完全不同：

### 5.1 分类总览

| 阻塞类型                  | 触发场景                                   | G 的状态                     | M 的状态          | P 的状态                       | G 被唤醒后去哪                 |
| --------------------- | -------------------------------------- | ------------------------- | -------------- | --------------------------- | ------------------------ |
| **channel/select 阻塞** | `ch <- data`（满）/ `<-ch`（空）/ `select{}` | `_Gwaiting`               | 不受影响，继续调度下一个 G | 不受影响                        | 原 P 的 runq（不保证是原 P）      |
| **网络 I/O 阻塞**         | `conn.Read()` / `conn.Write()`         | `_Gwaiting`（挂在 netpoller） | **不受影响**       | **不受影响**                    | netpoller 唤起 → 全局 runq   |
| **系统调用阻塞**            | 文件 I/O、`syscall.Syscall`、cgo 调用        | `_Gsyscall`               | **进入内核阻塞**     | **可能被 sysmon 夺走并 hand off** | 优先回原 P，否则放全局 runq        |
| **time.Sleep 阻塞**     | `time.Sleep(d)`                        | `_Gwaiting`（挂在 timer 堆）   | 不受影响           | 不受影响                        | timer 到期 → 全局 runq 或任意 P |

### 5.2 Channel 阻塞 —— 只阻塞 G，不阻塞 M

> **源码索引：** `runtime/chan.go` 中的 `chansend()` 和 `chanrecv()` [10]，以及 `runtime/proc.go` 中的 `gopark()` 和 `goready()` [1]。

channel 操作阻塞时，调用链路为：

```
ch <- data (channel 已满)
  └─ chansend()  → gopark()  → park_m()  → schedule()
```

`gopark` 的语义是"把我（当前 G）挂起来，让 M 去执行别的 G"：

1. 将 G 从当前 P 的 runq 中移除
2. 将 G 的状态从 `_Grunning` 切到 `_Gwaiting`
3. 将 G 挂在 channel 的 `sendq`（发送等待队列）上
4. `schedule()` 选取下一个 G 给当前 M+P 执行

> **设计理由：** M 和 P 不受影响——继续工作。这正是 GMP 要达成的效果：让阻塞只发生在用户态最小的执行单元（G）上，不浪费珍贵的内核线程（M）和调度资源（P）。

当接收方从 channel 取走数据（或发送方推入数据）时，`goready()` 被调用：

1. 将 G 从 channel 的等待队列中取出
2. 将 G 的状态切为 `_Grunnable`
3. `runqput()` 将 G 放入某个 P 的 runq
4. 如果 runq 放入的是当前 P 的 runq 且有空闲 P，可能触发 `wakep()` 唤醒一个空闲 M

### 5.3 系统调用阻塞 —— G + M 都阻塞，P 被 hand off

> **源码索引：** `runtime/proc.go` 中的 `entersyscall()`、`exitsyscall()`、`handoffp()`、`retake()`（sysmon 调用的部分）[1]。handoff 机制源自 Vyukov 设计文档 [4]。

系统调用与 channel 阻塞的根本区别在于：**OS 内核也会阻塞在调用它的线程上**。Go 无法让一个线程"一边等内核返回，一边继续执行别的 Go 代码"。所以处理方式完全不同：

**进入系统调用（entersyscall）：**

```go
// entersyscall() 的核心逻辑（简化）
func entersyscall() {
    // 1. 获取当前 M 和 P
    mp := getg().m
    pp := mp.p.ptr()

    // 2. 标记 G 和 P 的状态
    casgstatus(mp.curg, _Grunning, _Gsyscall)
    atomic.Store(&pp.status, _Psyscall)  // P 标记为"挂靠 syscall"

    // 3. 【设计理由】乐观策略：不是立即 hand off P
    //    对于极短的系统调用（如 futex wait，几微秒就返回），
    //    执行完整的 hand off 反而浪费。所以先保持 P 与 M 的绑定，
    //    留给 sysmon 来判断是否需要夺走 P
}
```

**sysmon 介入——夺走 P（handoffp）：**

> **设计理由：** sysmon 是后台监控线程，它独立于 GMP 运行（不需要 P），周期性地检查所有 P。当一个 P 处于 `_Psyscall` 状态过久时，sysmon 执行 `handoffp()`：把 P 从阻塞的 M 上抢走，交给另一个空闲（或新建）的 M。

sysmon 对系统调用的处理分两档：

| 条件                   | sysmon 行为                          | 设计理由                              |
| -------------------- | ---------------------------------- | --------------------------------- |
| `_Psyscall` 超过 ~20μs | 如果 P 的 runq 已空，不立即动手               | 对极短的系统调用（如 futex），容忍——反正 P 没有积压工作 |
| `_Psyscall` 超过 10ms  | **无论 runq 是否为空，直接 handoffp**——夺走 P | 超过 10ms 说明这已经不是"短调用"了，P 必须释放给其他 M |

**handoffp 的过程：**

```
handoffp() → startm() → 唤醒空闲 M（或创建新 M）→ 空闲 M 绑定 P → schedule()
```

**退出系统调用（exitsyscall）：**

> **设计理由：** 系统调用返回后的优先策略是"尝试回到原 P"——这维护了 cache 局部性。如果原 P 已被 sysmon 夺走，则退而求其次找其他空闲 P。都找不到时，G 进入全局 runq，M park 自己。

```go
// exitsyscall() 核心逻辑（简化）
func exitsyscall() {
    // 1. 先尝试快速路径：原 P 还在吗？
    if oldp := ...; oldp.status == _Psyscall {
        // 原 P 还在！直接恢复
        atomic.Store(&oldp.status, _Prunning)
        casgstatus(gp, _Gsyscall, _Grunning)
        return  // 继续执行，零中断
    }

    // 2. 慢路径：原 P 被抢走了，找其他空闲 P
    if pp := pidleget(); pp != nil {
        // 找到一个空闲 P
        atomic.Store(&pp.status, _Prunning)
        mp.p.set(pp)
        casgstatus(gp, _Gsyscall, _Grunning)
        return
    }

    // 3. 没有 P 可用——G 放全局队列，M park 自己
    globrunqput(gp)
    stopm()
    // M 被唤醒后重新走 schedule()
}
```

### 5.4 Timer 阻塞 —— 时间驱动的挂起

> **源码索引：** `runtime/time.go` 中的 timer 四叉堆实现。

`time.Sleep`、`context.WithTimeout` 等的底层都依赖 runtime timer：

1. G 被 `gopark` 挂起，放入 timer 堆中（按唤醒时间排序）
2. 最近的 timer 到期时间被传给 netpoller（作为 epoll_wait 的 timeout）
3. sysmon 和调度循环中的 `checkTimers()` 处理到期的 timer，调用 `goready` 唤醒对应的 G

### 5.5 为什么文件 I/O 比网络 I/O "重"

> **设计理由：** 这是一个很多 Go 程序员感到困惑的设计差异——为什么网络 I/O 只用阻塞 G，而文件 I/O 会连带阻塞 M？

不同操作系统对两类 I/O 的非阻塞支持不同：

| I/O 类型        | 非阻塞 + 多路复用 | Go 的处理方式                                              |
| ------------- | ---------- | ----------------------------------------------------- |
| **网络 socket** | ✅          | epoll/kqueue 注册到 netpoller → 只阻塞 G                    |
| **普通文件（磁盘）**  | ❌          | 即使设了 O_NONBLOCK，磁盘 I/O 在 Linux 上仍然可能阻塞线程 → **M 跟着阻塞** |

这不是 Go 的缺陷，而是操作系统层面的限制——Linux 的 epoll 对 regular file 永远返回就绪，实际上文件 I/O 总是同步的（除非使用 io_uring，Go 也正在逐步支持）。因此大量磁盘 I/O 会导致大量 M 被创建（因为不断有 M 阻塞在文件 I/O 上、新的 M 被创建来替代），这就是"大量读写文件导致线程数暴涨"的根本原因。Siebenmann 在博客中多次观察到这一行为 [7]。

### 本节要点

- gopark/goready 只阻塞 G，M 和 P 继续运转——这是"轻量并发"的核心机制
- 系统调用不同——M 无法不跟着阻塞。此时的设计关键不是"不让 M 阻塞"（做不到），而是"P 必须立刻流转"
- handoffp 是 sysmon 最关键的职责之一——它确保 P 不会跟着 M 一起"陪葬"
- 文件 I/O vs 网络 I/O 的差异根源于操作系统，不是 Go 的取舍——但 Go 正在通过 io_uring 等机制弥合这个差距

---

## 6. Netpoller 深度解析

> **本节回答的问题：** Go 为什么能做到"一个网络连接阻塞不影响其他 goroutine"？netpoller 在调度循环的哪个环节起作用？epoll/kqueue 是怎么跟 goroutine 调度粘合在一起的？

### 6.1 核心思想：让操作系统等，不要让线程等

> **设计理由：** 传统的"一个连接一个线程"模型中，线程 `read()` 阻塞在网络 socket 上时，线程本身被内核挂起。Go 的做法是：G 发出 `read()` → netpoller 注册 fd → G 挂起 → M 继续执行其他 G → fd 就绪 → netpoller 把 G 放回 runq。**"阻塞"从内核级下沉到了用户级——G 以为自己被阻塞了，但实际上 M 始终在工作。** [1][11]

### 6.2 底层实现：封装操作系统的 I/O 多路复用

| 平台              | 多路复用机制                      | Go 中的实现                      |
| --------------- | --------------------------- | ---------------------------- |
| Linux           | epoll                       | `runtime/netpoll_epoll.go`   |
| macOS / FreeBSD | kqueue                      | `runtime/netpoll_kqueue.go`  |
| Windows         | IOCP (I/O Completion Ports) | `runtime/netpoll_windows.go` |

Go 的网络库（`net` package）在创建 socket 时不使用阻塞模式——它使用 `syscall.SetNonblock()` 将 fd 设为非阻塞，然后注册到 netpoller。

**一次 `conn.Read()` 的完整流程：**

```
1. G1 调用 conn.Read()
   └─ net 包 → poll.FD.Read() → syscall.Read() (非阻塞)
        │
        ├─ 有数据 → 直接返回（最快路径，不经过 netpoller）
        │
        └─ 没数据 (EAGAIN) → netpollblock()
             ├─ gopark(G1)         // G1 进入 _Gwaiting，挂在 fd 上
             ├─ netpolladd(fd)     // fd 加入 epoll 监控
             └─ schedule()         // M 去执行 G2、G3...

2. ... 数据到达 ...

3. epoll_wait 返回 fd 就绪
   └─ netpoll() 被唤起
        ├─ netpollready(G1)       // G1 状态 → _Grunnable
        └─ runqput(G1)           // G1 放入 P 的 runq（或全局队列）

4. 后续某个调度回合，G1 被再次调度执行
   └─ 从 netpollblock 恢复 → 重新 Read → 这次有数据 → 返回给调用者
```

### 6.3 Netpoller 在调度循环中的位置

> **源码索引：** `runtime/proc.go` 的 `findrunnable()` 函数中，netpoller 的调用位于第三优先级。

```go
// findrunnable() 中的 netpoller 部分（简化逻辑）
func findrunnable() (gp *g, inheritTime bool) {
    // ...

    // 优先级 ③：检查 netpoller（在 work stealing 之前）
    if netpollinited() && netpollWaiters.Load() > 0 {
        // netpoll(0) 中的参数 0 = 非阻塞：不等待，有就绪 fd 就处理，没有就立即返回
        if list := netpoll(0); !list.Empty() {
            gp := list.Pop()            // 取第一个就绪的 G
            injectglist(list.Remaining()) // 其余放到全局 runq
            casgstatus(gp, _Gwaiting, _Grunnable)
            return gp, false
        }
    }

    // 优先级 ④：netpoller 也没货 → work stealing
    // ...
}
```

> **设计理由：** netpoller 的检查位置（在 work stealing 之前）体现了"先看看有没有 I/O 唤醒的 G，没有再偷别人的 G"的思路。网络 I/O 唤醒的 G 可能已经等了很久（甚至几个调度周期），优先调度它们能降低网络延迟。

**除了 findrunnable，sysmon 也在 netpoll：**

```go
// sysmon() 中的网络轮询（简化）
func sysmon() {
    // ...
    // 每 10ms 左右，sysmon 调用 netpoll(true)（阻塞模式，超时极短）
    // 将就绪的网络 G 注入全局 runq
    // 这保证了即使所有 P 都忙于计算，网络 I/O 也不会被饿死
    if lastpoll := atomic.Load64(&sched.lastpoll); lastpoll != 0 {
        if netpollinited() {
            list := netpoll(delay)
            injectglist(&list)
        }
    }
}
```

> **设计理由：** sysmon 的独立 netpoll 是一条"安全网"——调度循环中的 netpoll(0) 只在 M 空闲时触发。如果所有 M 都在密集计算，没有 M 进入 findrunnable，网络事件会堆积。sysmon 以 ~10ms 的间隔主动轮询，确保网络 I/O 不会因为 CPU 繁忙而被无限期搁置。Aspen-Go 论文 [9] 对此做了定量分析，建议将轮询间隔缩短到 ~100μs 以降低尾部延迟。

### 6.4 Netpoller 与 Timer 的协作

netpoller 还充当了 runtime timer 的"闹钟"：

1. 所有 timer（`time.Sleep`、`context.WithTimeout` 等）按到期时间组织在四叉堆中
2. 最近的到期时间被传给 `epoll_wait(timeout=next_timer_deadline)`
3. 这样，**netpoller 在不增加额外系统调用的情况下，同时处理了 I/O 就绪和 timer 到期**

这是一举两得的精妙设计——一次 `epoll_wait` 同时服务两个目的。

### 本节要点

- Netpoller 将 I/O 阻塞从内核级下沉到用户级——G 阻塞，M 不阻塞
- 它在调度循环中位于第三优先级（runq → global queue → **netpoll** → steal）
- sysmon 的独立 netpoll 是防止"全员忙于计算导致网络事件饿死"的安全网
- netpoller 和 timer 共用同一套 epoll_wait 机制，一次系统调用服务两个目的

---

## 7. sysmon 监控线程

> **本节回答的问题：** sysmon 是一个怎样的存在？为什么它不需要 P？它做了哪些"正常调度循环管不了"的事？

### 7.1 sysmon 的身份

> **源码索引：** `runtime/proc.go` 中的 `sysmon()` 函数。sysmon 在程序初始化阶段（`runtime.main` 之前）由 `newm(sysmon, nil, -1)` 启动，运行在一个独立的 M 上。

sysmon 最特殊的地方：**它运行时不绑定 P**。这意味着：

- 它不受 GOMAXPROCS 的限制——不会占用 P 从而减少并行度
- 它可以访问所有 P 的状态（通过 `allp` 数组），做全局性的监控和干预
- 它不需要 mcache（所以不需要 P）

### 7.2 自适应休眠

sysmon 的核心结构是一个无限循环 + 自适应延迟：

```
初始延迟 ~20μs
    │
    ├── 系统不忙 → 逐渐增大延迟（最多 ~10ms）
    │            → 省 CPU，不浪费在空转上
    │
    └── 系统忙（有 G 就绪/有 P 需要干预）→ 缩小延迟
                 → 保持响应速度
```

```go
func sysmon() {
    delay := uint32(0)
    for {
        if idle == 0 {  // 系统不空闲
            delay = 20    // 最小延迟 ~20μs
        } else if idle > 50 {
            delay *= 2    // 指数退避
            if delay > 10*1000 {
                delay = 10 * 1000  // 最大延迟 ~10ms
            }
        }

        // 实际休眠
        usleep(delay)

        // 执行各项检查...
    }
}
```

> **设计理由：** 为什么用自适应休眠而不是事件驱动？因为 sysmon 需要一个"无论如何都会定期运行"的保证——它不能依赖其他组件来通知它（万一那些组件本身出了问题呢？）。自适应延迟在响应速度和 CPU 开销之间做了平衡。[1]

### 7.3 sysmon 的五大职责

**① 抢占长时间运行的 goroutine（retake → preemptone）**

sysmon 遍历所有 P，检查每个处于 `_Prunning` 状态的 P 是否已连续运行同一个 G 超过 10ms。如果是：

- 通过 `preemptone()` 向对应的 M 发送 **SIGURG** 信号
- M 的信号处理器捕获 SIGURG → 保存 G 的寄存器 → 将 G 放回 runq → schedule() 选下一个 G

> 详细机制见第 8 章。

**② 夺回因系统调用阻塞的 P（retake → handoffp）**

sysmon 遍历所有 P，检查处于 `_Psyscall` 状态超过一定时间阈值的 P：

- 超过 ~20μs 且 runq 为空，或
- 超过 10ms（无论 runq 状况如何）

则执行 `handoffp()`——将 P 从阻塞的 M 上解绑：

1. P 的状态从 `_Psyscall` 切为 `_Pidle`
2. `startm()` 用这个空闲 P 唤醒（或创建）一个 M
3. M 拿到 P → `schedule()` 继续执行留在 P 里的 G

**③ 网络轮询兜底**

```go
// sysmon 中的 netpoll（简化）
if lastpoll := atomic.Load64(&sched.lastpoll); lastpoll != 0 {
    list := netpoll(delay)  // 阻塞式，最长 delay 时间
    injectglist(&list)      // 将就绪的 G 注入全局 runq
}
```

**④ 强制 GC**

如果程序超过 2 分钟没有发生 GC，sysmon 强制执行一次。防止"分配速率极低导致 GC 永不触发、内存缓慢增长"的边界情况。

**⑤ 唤醒 scavenger（内存返还 OS）**

sysmon 定期唤醒内存 scavenger，将空闲超过 5 分钟的物理内存页归还给操作系统——这对容器化部署尤其重要（如果 Go 程序不归还空闲内存，容器内存使用量只升不降）。

### 7.4 sysmon 与调度循环的分工

| 场景              | 调度循环自己能处理吗？        | sysmon 的角色              |
| --------------- | ------------------ | ----------------------- |
| G 运行超过 10ms     | ❌ 调度循环在 G 运行时失去控制权 | **sysmon 发信号强制切出**      |
| M 在 syscall 中阻塞 | ❌ M 阻塞在内核，调度循环无法触及 | **sysmon 夺走 P，重新分配**    |
| 全网忙于计算，无 M 空闲   | ❌ 无人调用 netpoll     | **sysmon 独立轮询 netpoll** |
| GC 长期不触发        | ❌ GC 靠分配触发         | **sysmon 强制 GC**        |
| 空闲内存不归还         | ❌ 非调度循环职责          | **sysmon 唤醒 scavenger** |

一句话总结：**调度循环管"正常情况下的高效流转"，sysmon 管"异常情况的兜底和纠正"**。两者配合，覆盖了调度器需要应对的全部场景。

### 本节要点

- sysmon 是一个不需要 P 的特殊 M——这使它成为"全局监督者"
- 它的核心循环使用自适应休眠：忙时 ~20μs，闲时指数退避到 ~10ms
- retake() → preemptone() / handoffp() 是它最关键的两个动作
- sysmon 是"安全网"——处理调度循环正常运转下顾及不到的异常和边界情况

---

## 8. 抢占式调度的演进

> **本节回答的问题：** Go 的抢占调度经历了哪两个阶段？协作式抢占的致命缺陷是什么？Go 1.14 的异步信号抢占是怎么实现的？为什么选 SIGURG 这个信号？safe-point 是什么，哪些地方不能抢占？

### 8.1 第一阶段：协作式抢占（Go 1.0 - Go 1.13）

> **参考资料：** Go 1.14 之前，抢占依赖于编译器和运行时的协作——Go 编译器在**每个函数调用的序言**（prologue）中插入栈检查代码。Clements 在提案 #24543 [5] 中详细分析了这一机制的缺陷。

**机制：**

```asm
// 每个 Go 函数开头，编译器插入类似这样的栈检查：
CMP SP, g.stackguard0     // 比较栈指针和警戒线
JGE ok                    // 没超标，正常执行
CALL runtime.morestack    // 超标了！进入 morestack
```

`stackguard0` 有两个用途：

- **栈扩容**：SP 接近栈底 → `morestack` 进行栈拷贝/增长
- **抢占检查**：`stackguard0` 被 sysmon 设为 `stackPreempt` → `morestack` 判断为"抢占"而非"扩容" → 调用 `gopreempt_m()` → 将 G 放回 runq → `schedule()`

**致命缺陷：密集循环无法抢占。**

```go
// 这个 goroutine 永远不会被抢占（Go 1.13 及以前）
func busyLoop() {
    for {
        // 没有任何函数调用！编译器不会插入栈检查。
        // sysmon 设置了 preempt 标记也无济于事。
        // 此 goroutine 会永远霸占当前 P。
    }
}
```

> **参考资料：** Clements 在提案 #24543 [5] 中指出：这不仅仅是理论问题。在生产环境中，一个 goroutine 在紧凑循环中自旋等待某个原子变量被另一个 goroutine 设置——但由于前者不包含函数调用，后者永远不会被调度，造成了**死锁**。延迟 STW（Stop The World）操作也由此产生——GC 必须等待所有 goroutine 到达 safe-point，而被密集循环霸占的 goroutine 永远到不了。

**另一个问题：延迟的 STW。**

GC 需要所有 goroutine 在 safe-point 停下。如果某个 goroutine 跑了很久也没有函数调用（或函数被 inlined），GC 就得等着——在 8 核机器上等一个 goroutine 看起来没什么，但在 128 核机器上，等最后一个 goroutine 停下可能意味着等很久。

### 8.2 为什么"在循环处插入抢占点"被否决？

Go 团队曾认真考虑过一个替代方案：在循环后边（back-edge）插入抢占检查（类似于在 `for` 的 `}` 处加检查）。原型实现后，基准测试显示 **几何平均 7.8% 的性能退化**。对于 Go 这种以性能著称的语言，这是不可接受的。

> **参考资料：** Proposal #10958（David Chase 主导）的 fault-based preemption 原型尝试了只在循环后边加一条指令，基准测试仍然显示几何平均 7.8% 的性能退化 [5]。最终论证：**没有运行时开销的方案才是好方案**——这指向了基于信号的异步抢占。

### 8.3 第二阶段：异步信号抢占（Go 1.14+）

> **参考资料：** Austin Clements 的 *Proposal: Non-cooperative goroutine preemption* [5] 是权威设计文档。核心思路：**像操作系统抢占线程一样，用信号打断正在运行的 goroutine。** 汇编实现在 `runtime/preempt.go` 和 `preempt_amd64.s` [12]。

**总体流程：**

```
sysmon 检测 G 运行超过 10ms
    │
    ▼
preemptone(P) ── 向 P 所绑定的 M 发送 SIGURG
    │
    ▼
M 接收到 SIGURG → sighandler → doSigPreempt
    │
    ▼
asyncPreempt (汇编) → 保存所有寄存器到 G 的栈上
    │
    ▼
asyncPreempt2 → mcall → preemptPark → schedule()
    │
    ▼
G 被标记为 _Gpreempted，放回 P 的 runq
下次再被调度时，从栈上恢复寄存器，从断点继续执行
```

**为什么选 SIGURG？**

> **设计理由（出处 [5]）：**
> 
> - SIGURG 在 POSIX 中用于"带外数据到达"，实际应用几乎不监听它
> - debugger（如 Delve）默认透传 SIGURG，不拦截。用 SIGALRM 或 SIGPROF 会与 profiler 冲突
> - 它可以"假触发"（内核可能在某些情况下自发产生 SIGURG）而不会有副作用
> - 在 macOS 等不支持 POSIX 实时信号的平台上，SIGURG 是可用的

### 8.4 Safe-Point 机制

> **设计理由（出处 [5]）：** 不是所有指令位置都可以安全抢占。GC 在扫描栈时需要知道哪些寄存器存的是指针、哪些是整数——这个信息（"liveness map"）只在特定点（safe-point）有定义。

**safe-point 的条件：**

1. 编译器对该位置有完整的栈和寄存器活性信息
2. 当前不处于"非抢占区"——如写屏障（write barrier）、`unsafe.Pointer` 到 `uintptr` 的转换中间

**三种 unsafe 情况及其处理：**

| 情况                           | 为什么 unsafe                                     | 处理方式                                   |
| ---------------------------- | ---------------------------------------------- | -------------------------------------- |
| **写屏障中**                     | GC 正在标记，此时中断会导致漏标                              | 标记基本块为不可抢占。如果信号恰好落在此处 → 恢复 G 继续执行，稍后再试 |
| **unsafe.Pointer → uintptr** | 指针对象的唯一引用暂时存在于普通整数寄存器中，GC 看不到                  | 包含此类转换的函数整体标记为不可抢占                     |
| **Range loop 的"越过边界"指针**     | `for i, v := range slice` 可能构造超出 slice 边界的临时指针 | Go 1.11 重写了 range loop 编译方式，不会构造越界指针   |

**当抢占信号落在 unsafe point：**

signal handler 检查当前 PC 是否在 safe-point 范围内。如果不在→**什么都不做，直接返回**。这看起来什么都没改变，但 sysmon 稍后会再次尝试抢占——因为 G 仍然在运行，10ms 后会再次收到 SIGURG。在大多数情况下，G 会很快离开 unsafe 区域进入 safe-point。

**保守式栈扫描（conservative inner-frame scanning）：**

对于被抢占的 goroutine，调用链上的所有栈帧分为两类：

- **最内帧（被信号打断的那一帧）**：使用保守扫描——把任何"看起来像堆指针"的值当作指针。可能产生少量"假指针"导致对象延迟回收，但不会导致悬垂指针。这一帧变化频繁，被"保留"的对象很快会在下次 GC 被正确回收
- **其他帧**：正常使用编译器的 liveness map（精确扫描）

> **设计理由：** 保守扫描仅用于最内帧——这平衡了实现复杂度和正确性。如果对所有帧都做保守扫描，堆开销会更大；如果要求所有位置都有精确 liveness map，二进制体积会膨胀约 5%。[5]

### 8.5 异步抢占的性能开销

**关键优势：零运行时开销。**

与"在循环处插入抢占检查"不同，信号抢占在正常执行路径上不增加任何额外指令。只有在 sysmon 判定需要抢占时，才会产生信号发送/接收的开销。在 goroutine 频繁自愿切换的正常程序里，异步抢占可能永远不会触发。

### 本节要点

- 协作式抢占的致命伤：无函数调用的密集循环永远无法被抢占 → GC 延迟 → 甚至死锁
- 基于信号的异步抢占实现了"真抢占"——任何位置（safe-point 内）都可被中断
- SIGURG 被选中因为：不被 debugger 拦截、应用层几乎不用、可容忍假触发
- safe-point / unsafe-point 的精细管理是正确性的保证——保守式栈扫描是务实折中
- 零运行时开销——异步抢占优于循环插桩方案的根本原因

---

## 9. 关键场景全程演练

> **本节回答的问题：** 把前八章的知识点串起来，看看几个关键场景从头到尾是怎么走的。

### 9.1 场景一：创建并调度一个 Goroutine

```
【时间线】

T0: G0 (main goroutine) 执行到 "go foo(x)"

T1: runtime.newproc(fn=foo, arg=x)
    └─ newproc1()
         ├─ 从当前 P 的 gfree 链表取一个空闲 G（或新建）
         ├─ 初始化 G：设置栈、sched.pc=fn、返回地址=goexit
         ├─ 将参数从当前栈拷贝到 G 的栈
         ├─ 将 G 状态设为 _Grunnable
         └─ runqput(G)
              ├─ 如果 runnext 为空 → G 进入 runnext
              └─ 如果 runnext 非空 → 旧 runnext 挤入 runq[tail]，G 进入 runnext
                   └─ 如果 runq 已满 (256) → 将 runq 的一半 G + 这个 G 批量放入全局 runq

T2: (可能在其他 M 上) schedule() 看到 runnext 有 G，取走
    或：schedule() 从 runq head 取 G
    或：findrunnable() → globrunqget() 从全局队列拉一批到本地
    或：findrunnable() → runqsteal() 从其他 P 偷

T3: execute(G, inheritTime=false)
    └─ setg(G)                // curg = G
    └─ gogo(&G.sched)         // 汇编跳转到 foo 的代码

T4: foo 开始执行...
```

### 9.2 场景二：Channel 阻塞与唤醒

```
【初始状态】P0 的 M 在执行 G1，G1 往一个已满的 buffered channel 写数据

T0: G1 → ch <- data
    └─ runtime.chansend()
         └─ sendq 不为空或有接收者可配对？→ 否
         └─ 缓冲区有空间？→ 否（已满）
         └─ gopark() → park_m()
              ├─ G1 状态: _Grunning → _Gwaiting
              ├─ G1 挂在 c.sendq 链表末尾
              └─ schedule() → 选 G2 执行

T1: (过了一段时间) G3 从同一个 channel 读数据
    └─ runtime.chanrecv()
         └─ 缓冲区有数据 → 取走一个
         └─ 检查 sendq → G1 在等待
         └─ goready(G1)
              ├─ G1 状态: _Gwaiting → _Grunnable
              └─ runqput(G1) → 放入某个 P 的 runq

T2: G1 再次被调度，从 chansend 的 gopark 调用处恢复
    └─ 这次缓冲区有空间了 → 写入成功 → 返回
```

### 9.3 场景三：系统调用 Hand Off 与恢复

```
【初始状态】P0 绑定 M0，M0 执行 G1。G1 调用 syscall.Read(fd=普通文件)

T0: G1 → syscall.Read()
    └─ entersyscall()
         ├─ G1 状态: _Grunning → _Gsyscall
         ├─ P0 状态: _Prunning → _Psyscall
         └─ 但 M0 仍绑定 P0（乐观策略：假设 syscall 很快就返回）

T1: sysmon 巡视，发现 P0 在 _Psyscall 停留超过 20μs
    └─ 检查 P0.runq 是否为空 → 为空（G1 阻塞前没留下活）
    └─ 不做 handoff（还在等——也许 syscall 很快就回来）

T2: sysmon 又巡视，发现 P0 在 _Psyscall 已超过 10ms
    └─ 不再等了！retake() → handoffp(P0)
         ├─ P0 与 M0 解绑
         ├─ P0 状态: _Psyscall → _Pidle
         └─ startm(P0)
              ├─ 从 midle 链表取一个空闲 M1（或新建）
              └─ M1 绑定 P0 → schedule() → 执行 P0.runq 中的 G2, G3...

T3: syscall.Read() 返回（比如过了 50ms）
    └─ exitsyscall()
         ├─ 尝试快速路径：原 P0 还在吗？
         │    └─ P0.status 现在是 _Prunning（被 M1 占用了）→ 不在了
         ├─ 尝试慢路径：有其他空闲 P 吗？
         │    └─ pidleget() → 刚好有 P2 空闲 → 拿 P2
         ├─ G1 状态: _Gsyscall → _Grunning
         └─ G1 继续执行（在 M0 上，P2 的上下文中）
```

### 9.4 场景四：Work Stealing 推演

```
【初始状态】GOMAXPROCS = 4，P0 P1 P2 P3。
P0 的 M 忙（执行 CPU 密集型 goroutine），P1 P2 P3 空闲。

T0: P1 的空闲 M 在 findrunnable() 中：
    ├─ runqget(P1) → 空
    ├─ globrunqget(P1) → 空
    ├─ netpoll(0) → 无就绪 FD
    └─ runqsteal(P1) → stealOrder 随机排列 [P2, P3, P0]
         ├─ P2.runq → 空（P2 也是空闲的）
         ├─ P3.runq → 空
         └─ P0.runq → 有 80 个 G 在等待！
              └─ runqgrab(P0) → 偷走 40 个 G（一半）
              └─ 放入 P1 的 runq

T1: P1 拿着偷来的 40 个 G 开始 schedule() → execute() → ...

T2: 同样的过程，P2 和 P3 也各自从 P0 偷走一部分 G
    最终 P0 的 runq 从 80 个被均匀分散到 4 个 P 上
```

### 9.5 场景五：异步抢占触发

```
【初始状态】P0+M0 执行 G1，G1 在一个没有函数调用的紧凑循环里跑了 8ms

T0: sysmon 巡视，P0 状态是 _Prunning，同一个 G 连续跑了 8ms
    └─ 还没到 10ms 的阈值 → 不动作

T1: sysmon 又巡视（~10ms 后），P0 上同一个 G 已跑了 18ms
    └─ retake() → preemptone(P0)
         └─ 向 M0 发 SIGURG

T2: M0 收到 SIGURG
    └─ sighandler → doSigPreempt
         ├─ 检查当前 PC 是否在 safe-point
         │    └─ 是！
         └─ asyncPreempt (汇编)
              ├─ 保存所有寄存器到 G1 栈上 (SP, PC, BP, 通用寄存器...)
              └─ asyncPreempt2 → mcall → preemptPark
                   ├─ G1 状态: _Grunning → _Gpreempted
                   ├─ runqput(G1) → 放回 P0 的 runq
                   └─ schedule() → 选 G2 执行

T3: 后续某个调度回合，G1 被再次从 runq 取出
    └─ execute(G1)
         └─ gogo(&G1.sched) → 从栈上恢复寄存器 → 从断点继续
         └─ G1 完全不知道发生了什么，从 "8ms + 被抢占" 处继续
```

### 本节要点

- 每个场景都是一条完整的 G → M → P 状态转换链
- entersyscall 的乐观策略 + sysmon 的兜底 handoff 是"快路径 vs 慢路径"的典型设计
- work stealing 的随机化 + 偷一半 = 无协调的分布式负载均衡
- 异步抢占让 G 对"自己被中断"完全无感知——它只是下次被调度时从断点继续

---

## 10. 设计哲学与总结

> **本节回答的问题：** 贯穿 GMP 设计的核心思想是什么？这些设计决策之间有什么内在联系？

### 10.1 局部性优先

GMP 几乎每一个设计决策都在优化局部性：

| 设计                     | 局部性收益                                               |
| ---------------------- | --------------------------------------------------- |
| P 持有 runq              | G 在同一 P 上连续执行，CPU cache 保持热度                        |
| runnext 机制             | 新创建的 G 立即执行——它很可能与当前 G 共享数据                         |
| exitsyscall 优先回原 P     | 避免刚出 syscall 的 G 数据在另一个核的 cache 里变冷                 |
| work stealing 从 tail 偷 | 被偷的 G 是队列里"待最久"的——它们的 cache 大概率已经冷了，被迁移到另一个 P 的损失最小 |
| 从全局队列批量拉取              | 摊销全局锁开销，之后多次调度走本地路径                                 |

### 10.2 Continuation Stealing（而非 Task Stealing）

> **参考资料：** Vyukov 的设计文档 [4] 中明确提到 Go 使用 continuation stealing。这一选择的语义是：当 `go f()` 发生时，**新 goroutine f 立即执行（如同函数调用），而调用者的"后续部分"（continuation）被放入 runq。**

```go
// continuation stealing 的效果：
go child()      // child 直接开始执行（在当前 M 上）
parent()        // "parent()" 这个承接被放入 runq
```

这样做的好处：

- **串行执行等价性**：单线程 Go 程序的行为与不使用 goroutine 的函数调用链完全一致
- **join 天然高效**：父 goroutine 的 continuation 大概率还在本地 runq 里，join 时 cache 很热
- **runq 的界**：continuation stealing 下 runq 的最大深度等于"递归深度 × 并发数"，而非"期间创建的全部 goroutine 数"。这天然限制了 runq 的增长

### 10.3 公平 vs 吞吐的平衡

- 每 61 次调度查全局队列 → **保证公平**，全局队列中的 G 不会被饿死
- sysmon 10ms 抢占 → **保证公平**，计算密集型 G 不能长期霸占 CPU
- 被抢占的 G 放回全局队列 → **惩罚机制**（类似于 CFS 的 vruntime），让长时间运行的 G 掉到全局队列，而非继续留在本地队列
- work stealing 偷一半的不偷一个 → **摊薄偷取开销**，提高吞吐

### 10.4 "不共享内存来通信" 如何映射到调度设计

GMP 调度器天然适配 channel 通信模型：

- **gopark/goready 的开销极低**：channel 操作导致的阻塞/唤醒不过是 G 在两个链表间的移动 + 一次原子状态切换，没有锁争用（channel 本身无锁或仅局部锁）
- **不需要处理"锁持有者在阻塞"的复杂情况**：如果是共享内存锁，被阻塞的 G 可能还持有锁，调度器需要处理优先级反转等复杂问题。channel 模型下，阻塞就是阻塞，不带来额外的副作用
- **调度器可以激进地做 handoff/steal**：因为 G 不会在阻塞时持有"需要被释放"的资源（如跨 Goroutine 的 Mutex 锁）

### 10.5 演进中的调度器

GMP 模型自 Go 1.1 奠定的骨架一直沿用至今，但细节仍在持续演进：

- **Go 1.14**：异步信号抢占 [5]，解决了协作式抢占的死穴
- **Go 1.19**：软内存限制 + GC 调优
- **Go 1.21**：runtime 可感知 cgroup CPU 限制
- **Go 1.23+**：实验性支持 `GOMAXPROCS` 按 P 的忙闲动态调整
- **未来 (io_uring)**：Go 正在探索将文件 I/O 也纳入 netpoller 类似的异步模型，弥合"网络 I/O vs 文件 I/O"的最后一公里差距

### 本节要点

- GMP 是"局部性"和"公平性"两个目标间的动态平衡
- continuation stealing 是 Go 调度器区别于大多数 work-stealing 系统的独特选择
- channel 通信模型与调度器的 gopark/goready 机制天然协同——这解释了"不共享内存来通信"为什么不仅仅是口号
- GMP 的骨架 10 年未变，但抢占、netpoller、cgroup 感知等细节仍在活跃演进

---

## 参考资料

### 一手资料（Go 源码与设计文档）

1. Go Runtime Source: `src/runtime/proc.go` — 调度器主循环、findrunnable、sysmon、entersyscall/exitsyscall
2. Go Runtime Source: `src/runtime/runtime2.go` — g、m、p、schedt 结构体定义
3. Go Runtime Documentation: `src/runtime/HACKING.md` — 官方 runtime 开发指南，G/M/P 概念与同步原语总览
4. Vyukov, D. (2012). *Scalable Go Scheduler Design Doc*. [Google Docs](https://docs.google.com/document/d/1TTj4T2JO42uD5ID9e89oa0sLKhJYD0Y_kqxDv3I3XMw) — GMP 模型的原始设计文档
5. Clements, A. (2019). *Proposal: Non-cooperative goroutine preemption*. [Go Issue #24543](https://github.com/golang/go/issues/24543) / [Design Doc](https://github.com/golang/proposal/blob/master/design/24543-non-cooperative-preemption.md) — 异步信号抢占的正式提案

### 深度技术分析

6. "The Scheduler" series. devblogs.sh. — 调度循环源码 walkthrough（schedule、findrunnable、execute、goexit 详解）
7. Siebenmann, C. *Wandering Thoughts* (utcc.utoronto.ca). — Go 运行时行为观察与深度分析系列
8. Kennedy, B. *Scheduling In Go* series (Ardan Labs). — 调度器工程实践与性能调优

### 学术论文

9. Guo, Y. et al. (2025). *Aspen-Go: Revisiting Asynchronous Preemption and Network Polling in Go*. USENIX NSDI '25. — sysmon 对网络延迟影响的定量分析与改进方案

### 延伸阅读

10. Go Runtime Source: `src/runtime/chan.go` — channel 实现，gopark/goready 的实际使用者
11. Go Runtime Source: `src/runtime/netpoll_epoll.go` / `netpoll_kqueue.go` — netpoller 的平台实现
12. Go Runtime Source: `src/runtime/preempt.go` / `preempt_amd64.s` — 异步抢占的汇编实现
13. Go Blog: *The Go Memory Model* — 理解调度与内存可见性的关系
14. Cox, R. (2013). *Go 1.1 Release Notes*. — GMP 模型正式发布的官方公告

---

> **版本说明：** 本文基于 Go 1.21+ 的 runtime 撰写。关键历史节点（Go 1.0 GM、Go 1.1 GMP、Go 1.14 异步抢占）在文中明确标注。如需查阅与本文对应的最新源码，请访问 [go.googlesource.com/go](https://go.googlesource.com/go) 的 `src/runtime/` 目录。
