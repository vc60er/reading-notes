# GMP 调度模型 · 问答录

> 本文是《Goroutine 调度模型（GMP）深度讲义》（`gpm-scheduling.md`）的伴生文件，记录阅读过程中提出的追问和深入讨论。每个问题标注了对应的讲义章节。

---

## Q1：上下文切换方面，协程和线程各需要做哪些工作？耗时范围是多少？

> 对应讲义 §1.2。原讲义中仅有一张简表，此问促使将其展开为完整的逐步分解。

### 线程上下文切换（内核级）—— 6 步分解

线程切换由操作系统内核的调度器触发（时间片耗尽、I/O 阻塞、锁等待等），全程在内核态完成：

```
用户态                内核态                    用户态
  │                     │                         │
  │──── (1) 陷入 ──────►│                         │
  │                  save_all                    │
  │                     │──── (2) 保存上下文 ────►│
  │                     │  通用寄存器 (RAX-R15)    │
  │                     │  段寄存器 (CS/DS/...)    │
  │                     │  FPU/SIMD 状态          │
  │                     │                         │
  │                     │──── (3) 选下一个线程 ───►│
  │                     │  CFS 红黑树 O(log n)    │
  │                     │                         │
  │                     │──── (4) 切换地址空间 ──►│
  │                     │  改 CR3 (跨进程时)      │
  │                     │  TLB 刷新               │
  │                     │                         │
  │                     │──── (5) 恢复上下文 ────►│
  │                     │                         │
  │◄─── (6) 返回 ───────│                         │
```

| 步骤 | 操作 | 耗时 | 细节 |
|------|------|------|------|
| **① 陷入内核** | 硬件保存 SS/RSP/RFLAGS/CS/RIP 到内核栈 → 切内核栈 | ~50-100 ns | 模式切换的必要代价。即使不做上下文切换，单纯一次 `getpid()` 系统调用也需此开销 |
| **② 保存当前线程寄存器** | 通用寄存器 (RAX-R15)、段寄存器存入 `thread_struct`；**FPU/SIMD 上下文**通过 XSAVE 指令保存 | ~30-80 ns（不含 FPU） | 通用寄存器约 160 bytes。FPU 上下文因指令集而异：SSE 约 512 bytes，AVX-512 可达 **~2.5 KB**。Linux 默认 lazy FPU saving——仅在确实被用过时才保存 |
| **③ 选择下一线程** | CFS 调度器从红黑树取 vruntime 最小的 `task_struct`，更新调度统计 | ~50-500 ns | O(log n)。高负载机器的 runqueue 较深时，此步骤显著增大 |
| **④ 切换地址空间** | 如果下一线程属于**不同进程**：修改 CR3 → 页表基址变更 → **TLB 全部失效**。同一进程则跳过 | 0（同进程）/ ~200-500 ns（跨进程） | 线程切换中最大的变量。同进程线程共享地址空间——CR3 不变，TLB 完全保留。现代 CPU 的 **PCID** 可标记 TLB 条目带进程标签，减轻但不消除跨进程开销 |
| **⑤ 恢复下一线程上下文** | 从 `thread_struct` 恢复通用寄存器；使用过 FPU → XRSTOR 恢复 | ~30-80 ns | 对称于步骤② |
| **⑥ 返回用户态** | `sysret`/`iretq`：从内核栈恢复用户态 RIP/CS/RFLAGS/RSP/SS | ~50-100 ns | 模式切换的"返程票" |

**线程切换总耗时：**

| 场景 | 典型耗时 | 决定因素 |
|------|---------|---------|
| **同进程线程**（最佳） | **1-3 μs** | 无 TLB flush，无 CR3 切换 |
| **跨进程线程** | **3-10 μs** | 多了 CR3 切换 + TLB 冷启动 |
| **含 AVX-512 上下文** | 以上 + **2-3 μs** | XSAVE 区域 2.5KB，线性增长 |

> 测量来源：`lmbench` 的 `lat_ctx` 在 Intel Xeon (Skylake) 上同进程线程切换约 1.2-1.5 μs，跨进程约 3-5 μs。

### Goroutine 上下文切换（用户级）—— 4 步分解

全程在 Go runtime 的用户态代码中完成，OS 完全不知情：

```
当前 G (curg)                     g0 (调度栈)                   下一个 G
     │                                │                            │
     │── (1) 保存到 gobuf ──────────►│                            │
     │   SP → g.sched.sp             │                            │
     │   PC → g.sched.pc             │                            │
     │   BP → g.sched.bp             │                            │
     │                                │                            │
     │── (2) 切到 g0 栈 ────────────►│                            │
     │   mcall(): 换 SP 而已          │                            │
     │                                │                            │
     │                                │── (3) 选下一个 G ────────►│
     │                                │   runqget(p): lock-free   │
     │                                │   环形缓冲 O(1)            │
     │                                │                            │
     │                                │── (4) gogo 跳转 ─────────►│
     │                                │   恢复 SP/PC/BP           │
     │                                │   JMP (不是 CALL)          │
```

| 步骤 | 操作 | 耗时 | 与线程的差异 |
|------|------|------|-----------|
| **① 保存当前 G 到 gobuf** | SP、PC、BP 三个值写入 `g.sched`（24 bytes） | ~10-20 ns | 线程保存全套通用寄存器(~160B)+可选FPU(~2.5KB)；goroutine 只需 3 个寄存器。编译器在 `gopark` 调用点已做 caller-saved 寄存器的溢出 |
| **② 切到 g0 栈** | `mcall(fn)`：一条 MOV 把 SP 从 curg 栈指向 g0 栈 | ~5 ns | 线程切换要改内核栈 + 页表；goroutine 只改一个 SP。g0 与 curg 在同一进程地址空间 |
| **③ 选下一个 G** | `schedule()` → `runqget(p)`：从 P 的 lock-free 环形缓冲取 G | ~20-50 ns | 线程要走内核 CFS O(log n)；goroutine 是 CAS 级原子操作 O(1)。99%+ 的调度走此热路径 |
| **④ gogo 恢复并跳转** | 从 gobuf 恢复 SP/PC/BP → **JMP** 到目标 PC | ~10-20 ns | JMP 不是 CALL——不压栈、不配对 RET，控制权彻底转移 |

**Goroutine 切换总耗时：**

| 场景 | 典型耗时 | 说明 |
|------|---------|------|
| **本地 runq 有 G（热路径，99%+）** | **~50-100 ns** | 全程约 5-8 条汇编指令 |
| **需要 work stealing** | **~200-500 ns** | 多了遍历其他 P 的 CAS 操作，纯用户态 |
| **需要从全局队列拉取** | **~300-800 ns** | 多了 `sched.lock` 互斥锁（用户态 futex 快速路径） |

> 测量来源：社区 benchmark（`github.com/tsuna/contextswitch` Go 适配版）在 Intel i9-13900K 上测得热路径约 80-120ns，work stealing 约 300-400ns。

### 同框对比

| 维度 | 线程切换 | Goroutine 切换 |
|------|---------|--------------|
| **模式切换（用户↔内核）** | 2 次，每次 ~50-100ns | **0 次** |
| **寄存器保存量** | 全套通用(~160B) + 可选 FPU(SSE ~512B, AVX-512 ~2.5KB) | 3 个值：SP, PC, BP（24B） |
| **调度算法** | CFS 红黑树 O(log n) | 环形缓冲 O(1)，lock-free CAS |
| **地址空间操作** | 跨进程时改 CR3 → TLB 全刷新 | 无（同一进程，CR3 不变） |
| **内核参与** | 完整路径：`schedule()`→`context_switch()`→`switch_mm()`→`switch_to()` | 零。一切在 `runtime/proc.go` 用户态完成 |
| **典型耗时** | **1-10 μs** (1000-10000 ns) | **~0.05-0.5 μs** (50-500 ns) |

**差距总结：常见场景（同进程线程 ~1.5μs vs 本地 runq goroutine ~80ns）约 18-20 倍。**

> 源码索引：`runtime/runtime2.go` — `gobuf` 结构体仅 6 个字段，切换时只写入核心 3 个。`runtime/asm_amd64.s` — `gogo()` 和 `mcall()` 是切换发生的汇编入口。

---

## Q2：Goroutine 的栈为什么可以这么小（2KB）？

> 对应讲义 §1.2。核心洞见：goroutine 的栈能这么小，不是因为用栈少——而是因为**栈会按需自动增长**。2KB 只是起点。

### 线程栈为什么必须大且固定

OS 线程的栈大小在创建时固定（Linux 默认 8MB），之后无法改变：
- 内核在创建线程时预留全部虚拟地址空间（`VM_GROWSDOWN`），物理页懒分配
- 但虚拟空间已承诺——无法"超出"
- 超出即栈溢出（SIGSEGV），进程崩溃
- 选择 8MB 这个"足够大"的值是为了覆盖绝大多数程序的调用深度——但对一个只打印 "hello" 的 goroutine 来说浪费了 99.97%

### Goroutine 的答案：按需增长

Go 运行时在每次函数调用前插入栈检查，自动触发增长。

**增长全链路：**

```
函数序言: CMP SP, g.stackguard0
              │
     ┌─────超标────────┐
     │                  │
  JGE ok           CALL morestack
     │                  │
  正常执行          newstack()
                      │
                 ┌────┴────┐
                 │ 栈够大？  │
                 │ < 1GB   │
                 └────┬────┘
                      │ 是
                 stackalloc(新栈)
                 大小 = 当前栈 × 2
                 (优先从 P.mcache 取)
                      │
                 copystack()
                 ├─ memmove 搬全部字节到新栈
                 ├─ 调整所有指向旧栈的指针
                 └─ g.stack = 新栈 {lo, hi}
                      │
                 stackfree(旧栈)
                 (归还 per-P cache)
                      │
                 gogo → 继续执行
```

**关键数据：**

| 参数 | 值 | 来源 |
|------|---|------|
| 初始栈大小 | 2 KB | `_StackMin` (`runtime/stack.go`) |
| 最大栈 | 1 GB | `maxstack` |
| 增长策略 | 翻倍（2KB→4KB→8KB→...） | `newstack()` |
| 收缩条件 | 实际使用 < 栈大小 / 4 | GC 阶段触发 |
| 收缩策略 | 缩小到当前的一半 | `shrinkstack()` |
| 分配路径（热） | P 的 `mcache.stackcache`，lock-free | `stackalloc()` |

### 分段栈 → 连续栈：Hot Stack Split 的教训

Go 1.0-1.2 使用的是**分段栈**（segmented stacks）：不够用时分配新段链接到旧段。

在以下场景中暴露了致命缺陷：

```
A → B → A → B → A → B → ...（循环调用）

如果 A 和 B 各自的栈帧恰好在"刚好不够"的边界附近：
  每次 A→B → 跨段 → new segment → 分配
  每次 B→A → 返回上一段 → 释放
  循环 100 万次 → 分配/释放 100 万次

这就是 "hot stack split"
```

Brad Fitzpatrick (Go team) 在 2016 年 Gophercon India 回忆："这在大多数时候运行得很好——直到它不好为止。你在一段紧密循环中在栈段之间来回弹跳，付出巨大的性能代价。"

**Go 1.3** 引入连续栈+拷贝方案，**Go 1.4** 彻底删除分段栈代码。

### 为什么 OS 线程做不到？

|  | Goroutine 栈 | OS 线程栈 |
|---|---|---|
| **溢出检测** | 编译器插入的 SP vs stackguard 检查 | 内核 guard page（访问即 SIGSEGV） |
| **扩容机制** | Go runtime：`copystack()` | 无——创建时大小固定 |
| **为什么不能扩** | — | 内核无法安全地暂停线程、找到该线程栈上所有指针并移动它们——需要**编译器的精确栈指针映射** |
| **关键能力** | 编译器生成精确栈扫描信息，runtime 用这些信息安全调整所有指向栈的指针 | 操作系统对栈内容的语义一无所知 |

### 实际影响

- 10 万个 goroutine × 2KB 初始 = **~200 MB** 栈内存
- 10 万个线程 × 8MB = **~800 GB** 栈内存（物理上不可行）

这形成了完整的闭环：**2KB 起步 → 翻倍增长至所需大小 → 使用量下降时收缩（GC 触发，<1/4 时缩为一半）**。任何时刻，goroutine 的栈都接近它实际需要的尺寸。

---
---

> **关联文件：** 本文的讨论已整合入 `gpm-scheduling.md` 的 §1.2 中。本文是该节原始展开的独立存档，便于单独查阅。

---

## Q3：创建 goroutine 时为什么要切到 g0（线程栈）上执行？具体是怎么实现的？

> 对应讲义 §3.2（M 的 g0）。从 `go func()` → `newproc` → `systemstack` → `newproc1` 的完整链路，以及 `systemstack` 的汇编实现细节。

### 为什么必须切到 g0 栈？

从 `go func()` 到实际创建 goroutine 的调用链：

```go
// runtime/proc.go
func newproc(fn *funcval) {
    gp := getg()               // 当前正在执行的 G
    pc := getcallerpc()        // go 语句的程序计数器

    systemstack(func() {       // ★ 切换到 g0 栈
        newg := newproc1(fn, gp, pc)   // 核心创建逻辑
        pp := getg().m.p.ptr()
        runqput(pp, newg, true)        // 放入运行队列
        if mainStarted {
            wakep()
        }
    })
}
```

`newproc1` 被 `systemstack` 包裹，强制在 g0 上执行。四条理由：

#### 理由一：g0 栈不可增长——避免无限递归

```
如果 newproc1 在 curg 栈上执行：
  curg 栈快满了 → 函数序言触发 morestack → newstack()
    → newstack 需要分配新栈 → stackalloc()
      → stackalloc 内部的函数调用也有序言
        → 序言又检查 stackguard0 → 栈还是不够 → 又触发 morestack
          → ... 无限递归，最终 fatal("stack overflow")
```

g0 栈是**固定大小（~8KB）、不可增长**的。所有 runtime 核心函数（`newstack`、`schedule`、`newproc1` 等）必须在 g0 上运行，因为**栈扩容本身需要调度器配合，调度器不能在自己扩容的同时处理扩容**——这是一个鸡生蛋蛋生鸡的死锁。

#### 理由二：不可抢占——保证数据结构的原子操作安全

`newproc1` 操作的关键数据结构：

```
newproc1 的执行路径：
  → gfget(pp)            // 从 P.gfree 链表取空闲 G（CAS 原子操作）
  → malg(_StackMin)      // 如果链表为空，分配新的 G+栈
  → memmove              // 拷贝函数参数到新 G 的栈上
  → runqput(pp, newg)    // 放入 P.runq（CAS 原子操作）
```

如果在这些操作中间收到 SIGURG 抢占信号，P 的 gfree 链表或 runq 可能处于**半修改状态**——比如 gfree 的某个节点的 next 指针已经被改但 head 还没更新。g0 上的代码不会响应抢占（`m.locks++` 阻止），保证了这些关键区的原子性。

#### 理由三：当前 G 的栈可能只有 2KB

用户 goroutine 初始栈仅 2KB。`newproc1` 内部会调用 `stackalloc` 给新 G 分配初始栈，而 `stackalloc` 对于大栈需要从 mheap 分配、可能触发更深的调用链。2KB 根本不够用。

#### 理由四：g0 不受 GC 写屏障干扰

```
GC 的写屏障规则：
  用户 goroutine 栈上写入指针 → 需要写屏障（标记该指针指向的对象为存活）
  g0 栈 → 不受写屏障扫描，runtime 可在 GC 关键区安全执行
```

`newproc1` 在创建 G 的过程中会操作 `g.sched`、`g.stack` 等字段，其中包含指针。如果这些操作发生在用户 G 栈上且 GC 正在进行，可能需要写屏障——但写屏障本身对栈状态有假设。在 g0 上执行彻底绕开了这个问题。

### systemstack 的实现——逐行拆解

`systemstack(fn)` 的语义是：**临时**切到 g0 栈执行 fn，fn 执行完**返回**到 curg 栈继续。

```
与 mcall 的区别：
  systemstack:  curg → g0 → 执行 fn → 返回 curg  ← 临时切换，有来有回
  mcall:        curg → g0 → 执行 fn → fn 不返回，直接进入 schedule()  ← 永久切换
```

#### amd64 汇编实现

```asm
// runtime/asm_amd64.s (简化注释版本)
TEXT runtime·systemstack(SB), NOSPLIT, $0-8
    MOVQ    fn+0(FP), DI        // DI = 要执行的闭包函数指针

    // ❶ 获取当前 G 和 M
    get_tls(CX)                 // CX = TLS 基址（Go 用 FS 段寄存器指向 TLS）
    MOVQ    g(CX), SI           // SI = g（当前 goroutine，可能是 curg 也可能是 g0）
    MOVQ    g_m(SI), AX         // AX = m（当前 M）

    // ❷ 已经在 g0 上了？直接调用
    MOVQ    m_g0(AX), DX        // DX = g0
    CMPQ    SI, DX              // 当前 g == g0 ?
    JEQ     noswitch            // 是 → 不切换，直接 CALL fn

    // ❸ 不在 g0 → 需要切换
    // 第一步：保存 curg 的返回信息
    MOVQ    SI, m_curg(AX)      // m.curg = 当前 G（确认绑定关系）
    MOVQ    SP, (g_sched+gobuf_sp)(SI)   // curg.sched.sp = 当前栈指针
    MOVQ    $return, (g_sched+gobuf_pc)(SI) // curg.sched.pc = return 标签的地址

    // 第二步：切换到 g0 栈
    MOVQ    DX, g(CX)           // TLS.g = g0  ← 告诉 runtime "现在跑的是 g0"
    MOVQ    (g_sched+gobuf_sp)(DX), SP   // SP = g0.sched.sp ← 切栈！从这里开始就在 g0 栈上了

    // ❹ 在 g0 栈上调用 fn
    CALL    DI                  // 调用 newproc 传入的闭包（在 g0 栈帧上）
    
    // ❺ 这不是 CALL 的返回，而是从 curg.sched.pc 跳过来
return:
    get_tls(CX)
    MOVQ    g(CX), SI           // SI = g0（此时 TLS 里还是 g0）
    MOVQ    g_m(SI), AX         // AX = m
    MOVQ    m_curg(AX), SI      // SI = curg ← 找到原来的 G

    // ❻ 切回 curg 栈
    MOVQ    SI, g(CX)           // TLS.g = curg  ← 恢复身份
    MOVQ    (g_sched+gobuf_sp)(SI), SP   // SP = curg.sched.sp ← 切回 curg 栈！

    RET                         // 返回到 newproc 中 systemstack 的下一行
```

#### 分步图解

```
时间线：从 newproc 调用 systemstack 开始到返回

     curg 栈 (2KB-1GB)                  g0 栈 (~8KB, 固定)
    ┌────────────────┐                ┌────────────────┐
    │                │                │                │
    │  newproc()     │                │                │
    │  systemstack───┼──┐             │                │
    │  ...           │  │   ┌────────►│  fn() {        │
    │                │  │   │         │    newproc1()  │
    │                │  │   │         │    runqput()   │
    │                │  │   │         │  }             │
    │  ◄─────────────┼──┼───┘         │                │
    │  继续执行...    │  │             │                │
    │                │  │             │                │
    └────────────────┘  │             └────────────────┘
                         │
    ① SP = curg 栈顶     │  ③ CALL DI: fn 在 g0 栈上
    ② 保存 curg.sched   │     创建栈帧、执行
       .sp = curg_SP    │
       .pc = return     │  ④ fn 返回 → 弹栈帧
    ② TLS.g = g0        │     → 执行 return: 标签
    SP = g0.sched.sp ───┘     → 恢复 TLS.g = curg
                              → SP = curg.sched.sp
                              → RET 回到 newproc
```

#### 关键细节：为什么 `return:` 标签能被正确跳转到？

```
curg.sched.pc 被设为 $return（return 标签的地址）
                            ↑
当 fn 执行完毕，CALL DI 返回后，CPU 继续往下跑，自然到达 return: 标签
随后 switch back to curg 栈，执行 RET

这不是一个"从某个地方跳回来"的机制
而是：
  保存 → 切栈 → 调用 → fn返回 → 恢复 → RET
   ↑                                    ↑
   在 curg 栈上保存返回点              在 curg 栈上执行真正的返回
```

`return` 标签处的代码仍然在 **g0 栈**上运行，直到执行了 `MOVQ (g_sched+gobuf_sp)(SI), SP` 才真正切回 curg 栈。然后用 `RET` 弹栈返回——因为 `RET` 是从栈上弹出返回地址并跳转，而此时 SP 已经指向 curg 栈，弹出来的是 curg 调用链上的返回地址。

#### 快速路径：已经跑在 g0 上

`systemstack` 可以嵌套——g0 上的代码也可以调用 `systemstack`：

```go
// schedule() 在 g0 上运行，内部调 execute()，execute 也调了 systemstack
func execute(gp *g, inheritTime bool) {
    systemstack(func() {
        // 检测到已经在 g0 → JEQ noswitch → 直接 CALL，零开销
    })
}
```

```asm
noswitch:
    CALL    DI       // 直接调用，不切栈
    RET              // 直接返回
```

### 反过来：新 goroutine 第一次被调度时怎么"离开" g0？

当 `schedule()` 选中新创建的 G：

```go
// schedule() 运行在 g0 上
func schedule() {
    gp, inheritTime = findrunnable()  // 找到了 newproc1 创建的 G
    execute(gp, inheritTime)
}

func execute(gp *g, inheritTime bool) {
    mp := getg().m
    mp.curg = gp           // M 绑定新 G
    gp.m = mp
    gp.stackguard0 = gp.stack.lo + _StackGuard
    gogo(&gp.sched)        // ★ 从 g0 栈跳转到新 G 的栈，永不返回
}
```

`gogo` 汇编：

```asm
TEXT runtime·gogo(SB), NOSPLIT, $0-8
    MOVQ    buf+0(FP), BX        // BX = &gp.sched
    MOVQ    gobuf_g(BX), DX      // DX = gp
    MOVQ    0(DX), CX            // CX = gp 的状态（atomicstatus）
    MOVQ    gobuf_sp(BX), SP     // ★ SP 从 g0 栈 → 新 G 的栈
    MOVQ    gobuf_pc(BX), BX     // BX = 入口函数地址（newproc1 设定的）
    JMP     BX                    // ★ 跳转到用户函数！不是 RET，不回到 g0
```

### 两条核心栈切换路径对比

| 维度 | `systemstack` | `gogo` |
|------|-------------|--------|
| **方向** | curg → g0（借道） | g0 → curg（交棒） |
| **切换方式** | CALL → 返回 → 恢复 → RET | JMP（永不返回） |
| **调用者** | `newproc`、`execute` 等 runtime 函数 | `execute`（schedule 循环内） |
| **目的** | 在安全环境执行 runtime 关键代码 | 把控制权永久交给用户 goroutine |
| **返回行为** | fn 执行完，切回 curg，继续执行 | 不返回——gogo 之后就是用户代码 |

### 一句话总结

> **`systemstack` 让 runtime 关键代码跑在 g0 的"安全区"（固定栈 + 不可抢占 + 无 GC 干扰），执行完切回 curg；而 `gogo` 是把控制权从 g0 永久交给新 goroutine——JMP 而非 CALL，不再回头。**

---

---

## Q4：goroutine 在六种阻塞/切出场景下如何调度？G 进入什么状态、存放在哪个队列？

> 对应讲义 §5（阻塞处理全景）和 §8（抢占式调度）。将六种常见场景——执行超时抢断、网络 I/O、系统调用、锁、sleep、channel——统一按"触发链路 → G 状态 → 存放队列 → M/P 是否受影响"梳理。

### 总表

| 场景 | 触发机制 | G 状态变化 | G 存放在哪里 | M 状态 | P 状态 |
|------|---------|-----------|-------------|--------|--------|
| ① 执行超时（信号抢占） | sysmon → SIGURG → `asyncPreempt` | `_Grunning` → `_Gpreempted` | **放回原 P 的 runq**（`preemptPark` → `runqput`） | 不受影响 | 不受影响 |
| ② 网络 I/O 阻塞 | `conn.Read()` → EAGAIN → `netpollblock` → `gopark` | `_Grunning` → `_Gwaiting` | **挂在 netpoller 的 fd 等待链表**上 | 不受影响，继续调度 | 不受影响 |
| ③ 文件 I/O / 系统调用 | `syscall.Read()` → `entersyscall` | `_Grunning` → `_Gsyscall` | **不在任何 runq**（G 随 M 一起"冻结"在内核中） | **进入内核阻塞** | 乐观保持绑定，超时后 sysmon **夺走 handoff** |
| ④ 锁阻塞（sync.Mutex） | `Lock()` 抢不到 → 自旋 → `gopark` | `_Grunning` → `_Gwaiting` | **挂在 semaTable 的信号量等待链表**上 | 不受影响 | 不受影响 |
| ⑤ time.Sleep | `time.Sleep()` → `gopark` | `_Grunning` → `_Gwaiting` | **挂在 runtime timer 四叉堆**上 | 不受影响 | 不受影响 |
| ⑥ channel 阻塞 | `ch<-`（满）/ `<-ch`（空）→ `gopark` | `_Grunning` → `_Gwaiting` | **挂在 channel 的 sendq/recvq 等待链表**上 | 不受影响 | 不受影响 |

### ① 执行超时（信号抢占）—— Go 1.14+

```
sysmon 检测同一 G 连续运行 > 10ms
  → preemptone(P) → 向 M 发 SIGURG
    → sighandler → doSigPreempt → asyncPreempt(汇编)
      → 保存所有寄存器到 G 栈上
        → asyncPreempt2 → mcall → preemptPark
          → G 状态: _Grunning → _Gpreempted
          → runqput(G) → 放回当前 P 的 runq
          → schedule() 选下一个 G
```

- `_Gpreempted` 在行为上与 `_Grunnable` 等同——下次被取走执行时，通过 `gogo` 从保存的寄存器断点无缝恢复
- M 和 P 完全不受影响，继续干活

### ② 网络 I/O 阻塞 —— 只阻塞 G，不阻塞 M

```
G1 调用 conn.Read()
  → net 包 → poll.FD.Read() → syscall.Read() 非阻塞模式
    → 没数据 (EAGAIN)
      → netpollblock()
        → gopark(G1)          → G1: _Grunning → _Gwaiting
        → netpolladd(fd)      → fd 注册到 epoll
        → G1 挂在 fd 的等待链表上
        → schedule()          → M 去执行 G2、G3...
```

**唤醒路径：**

```
数据到达 → epoll_wait 返回
  → netpoll() 被调用 (findrunnable 优先级③ 或 sysmon 兜底)
    → netpollready(G1)
      → G1: _Gwaiting → _Grunnable
      → 放入全局 runq（因为原 P 此时可能已绑定其他 M）
```

> 这是 Go "一个连接阻塞不影响其他 goroutine" 的根源——阻塞发生在用户态。

### ③ 文件 I/O / 系统调用 —— M 也跟着阻塞

```
G1 调用 syscall.Read(fd=普通文件)
  → entersyscall()
      ├─ G1: _Grunning → _Gsyscall
      ├─ P:  _Prunning → _Psyscall   (乐观策略：暂时保持 M↔P 绑定)
      └─ M0 进入内核阻塞（磁盘 I/O 没有非阻塞模式）
```

**sysmon 介入：**

```
~20μs 后 → P.runq 为空 → 不急于动手（万一 syscall 马上返回）
~10ms 后 → retake() → handoffp(P)
    ├─ P 与 M0 解绑 → P: _Psyscall → _Pidle
    └─ startm(P) → 空闲/新建 M1 绑定 P → schedule()
```

**系统调用返回（exitsyscall）：**

```
① 快速路径：原 P 还在？→ 是 → 直接恢复 G1 继续执行（零开销）
② 慢路径：原 P 被抢走但有其他空闲 P → 绑定新 P → G1: _Gsyscall → _Grunning
③ 最慢路径：没有空闲 P → G1: _Gsyscall → _Grunnable → globrunqput(G1)
              → M0 park 自己 (stopm)
```

> P 的 handoff 机制是防止"P 跟着 M 陪葬"的关键。

### ④ 锁阻塞（sync.Mutex）

```
G1 执行 mu.Lock()，锁已被 G2 持有
  → runtime.lock2() 自旋（~30 个 PAUSE 周期）
    → 还是拿不到
      → semacquire1()
        → gopark()              → G1: _Grunning → _Gwaiting
        → G1 挂在 semaRoot.treap 的信号量等待链表上
        → schedule()

G2 执行 mu.Unlock()
  → semrelease()
    → goready(G1)
      → G1: _Gwaiting → _Grunnable
      → runqput(G1) → 放入某个 P 的 runq
```

- G 存放在 **semaTable 的信号量等待队列**（251 个 semaRoot，按地址 hash，每个内部是 treap）
- 在 `gopark` 之前有一段**用户态自旋**——乐观策略，如果锁很快就释放了，连 gopark 的开销都省了

### ⑤ time.Sleep

```
time.Sleep(5 * time.Second)
  → runtime.timeSleep()
    → 创建 timer 节点，记录唤醒时间
    → gopark()                → G: _Grunning → _Gwaiting
    → G 挂在四叉堆 timer 堆上（按到期时间排序）
    → schedule()

timer 到期:
  → checkTimers() (在 schedule() 循环或 sysmon 中调用)
    → goready(G) → G: _Gwaiting → _Grunnable → 放入 runq
```

- netpoller 的 `epoll_wait(timeout=next_timer_deadline)` 同时充当 timer 的"闹钟"——一次系统调用服务两个目的
- 唤醒后一般放入**全局 runq**（原 P 可能早已忙于其他 G）

### ⑥ Channel 阻塞

```
发送阻塞（channel 已满）:
G1: ch <- data → chansend() → gopark()
  → G1: _Grunning → _Gwaiting → 挂在 c.sendq 链表末尾

接收阻塞（channel 为空）:
G1: <-ch → chanrecv() → gopark()
  → G1: _Grunning → _Gwaiting → 挂在 c.recvq 链表末尾

唤醒:
对方操作 → 从 sendq/recvq 取出 G → goready(G)
  → G: _Gwaiting → _Grunnable → runqput → 放入当前操作方所在 P 的 runq
```

### 调度之后 G 的去向：三类队列

| 队列类型 | 存放内容 | 存取方式 |
|---------|---------|---------|
| **P 的本地 runq**（环形缓冲 256 + runnext） | `_Grunnable` 的 G，等待被执行 | lock-free CAS，O(1) |
| **全局 runq**（`sched.runq`） | P 本地队列满时溢出的 G，或 netpoller/timer 唤醒的 G | 需要 `sched.lock` |
| **阻塞等待队列**（channel/sema/timer/netpoller） | `_Gwaiting` 的 G，等待特定条件满足 | 条件满足时 `goready()` → 移入 runq |

**核心规律：**
- `_Gwaiting` → 放在**等待队列**中（channel/sema/timer/netpoller）
- `_Gsyscall` → **不在任何队列**中（随 M 陷入内核）
- `_Gpreempted` / 被唤醒 → 放入 **runq**（本地优先，其次全局）
- `_Gdead` → 放入 **gfree 空闲链表**（供复用）

---

## Q5：Golang 栈内存的实现细节是什么？与线程栈的对比？

> 对应讲义 §1.2 及 Q2。Q2 已覆盖"栈为什么能这么小"的核心论据，本条补充**栈的分配路径、收缩机制、物理布局、copystack 指针调整**等实现细节，并与线程栈做系统对比。

### 1. Goroutine 栈的物理形态：连续栈 + 拷贝增长

Go 1.3 之前用**分段栈**（linked list of segments），Go 1.3 改为**连续栈 + 拷贝**，Go 1.4 彻底删除分段栈代码。

```
Goroutine 栈在内存中的布局：

  g.stack.hi  ┌────────────────────┐  ← 高地址（栈底，初始 SP 在这里）
              │   已使用的栈帧       │  ← 函数调用链的实际占用
              │   (used frames)    │
      SP ────►├────────────────────┤  ← 当前栈指针
              │   未使用（可增长）    │
              │   stackguard0 警戒线 │  ← SP 低于此值触发 morestack
  g.stack.lo  └────────────────────┘  ← 低地址（栈顶）
```

关键数据结构（`runtime/runtime2.go`）：

```go
type stack struct {
    lo uintptr  // 低地址
    hi uintptr  // 高地址——SP 从 hi 向 lo 增长
}

type g struct {
    stack       stack   // 当前栈边界
    stackguard0 uintptr // 栈溢出警戒线
    stackguard1 uintptr // g0 栈用，不需要抢占检查
}
```

### 2. 栈增长全链路

#### 编译器插桩

编译器在**每个 Go 函数序言**插入：

```asm
CMP  SP, g.stackguard0    // SP < 警戒线？
JGE  ok                   // 否 → 安全
CALL runtime.morestack    // 是 → 栈不够了！
ok:
    // 正常执行
```

`stackguard0` 通常设为 `g.stack.lo + StackSmall`（~128 bytes 预留），在栈真正溢出前触发扩容，留出余量给 `morestack` 自身安全执行。

#### morestack → newstack → copystack

```
CALL morestack (汇编入口)
  → mcall → 切换到 g0 栈
    → newstack()
        ├─ 是抢占信号？(stackguard0 == stackPreempt)
        │   → gopreempt_m() → schedule()
        │
        ├─ 当前栈大小 < 1 GB (maxstack)？
        │   ├─ 是 → 新大小 = 当前 × 2
        │   │       2KB → 4KB → 8KB → 16KB → 32KB → ...
        │   └─ 否 → fatal("stack overflow")
        │
        ├─ stackalloc(新大小)
        │   └─ 小栈（< 32KB）：P.mcache.stackcache → lock-free
        │   └─ 大栈（≥ 32KB）：mheap → 需要全局锁
        │
        ├─ copystack(gp, newStack)
        │   ├─ memmove: 整块拷贝旧栈到新栈
        │   ├─ adjustpointers: 遍历帧，修正所有指向旧栈的指针
        │   │   (利用编译器生成的 liveness map)
        │   └─ gp.stack = {new_lo, new_hi}
        │
        └─ stackfree(旧栈) → 归还到 P 的 stackcache
```

#### copystack 指针调整原理

```
旧栈 (2KB)                      新栈 (4KB)
┌────────────┐                  ┌────────────┐
│  frame 3   │  ← 局部变量      │  frame 3   │  ← 指针值已更新
│  *x = 0x.. │    指向 frame 1  │  *x = 0x.. │
├────────────┤                  ├────────────┤
│  frame 2   │                  │  frame 2   │
├────────────┤                  ├────────────┤
│  frame 1   │  ← 被指向的变量   │  frame 1   │  ← 变量已搬到新地址
└────────────┘                  └────────────┘

adjustpointers:
  1. 遍历 goroutine 调用栈的每一帧
  2. 对每一帧，查编译器生成的 stack map（哪些位置存的是指针）
  3. 如果指针值在 [old_lo, old_hi] 范围内 → 加 delta = new_lo - old_lo
  4. G.sched 中保存的 SP/BP/PC 也做相应调整
```

> 这是 OS 无法做到的关键——只有编译器才能精确区分栈上的指针和整数。

### 3. 栈的收缩

GC mark termination 阶段触发：

```go
func shrinkstack(gp *g) {
    usedSize := gp.stack.hi - gp.sched.sp
    oldSize  := gp.stack.hi - gp.stack.lo

    if usedSize < oldSize / 4 {   // 使用量 < 1/4
        newSize = oldSize / 2      // 缩小为一半
        if newSize < _StackMin {
            newSize = _StackMin    // 最小 2KB
        }
        copystack(gp, 新栈)         // 拷贝到更小的栈
    }
}
```

**只能发生在 GC safe-point**——shrinkstack 需要扫描和调整指针，必须在 GC 一致状态下进行。

### 4. 栈的分配与复用

```
stackalloc(n bytes)
  ├─ 小栈（< 32KB，固定级别 2K/4K/8K/16K）:
  │   → P.mcache.stackcache[class].list → lock-free！
  │   → 为空时从 mheap 批量补充
  │
  └─ 大栈（≥ 32KB）:
      → mheap → 需要 mheap.lock
```

G 结构体本身也是池化的：

```go
// newproc1: 从 gfree 链表取
newg := gfget(pp)         // P 本地 gfree，lock-free
if newg == nil {
    newg = malg(_StackMin) // 新建 G + 2KB 栈
}

// goexit0: 归还到 gfree
gfput(pp, gp)             // G 放回 P 的 gfree 链表
```

一个 "G + 2KB 栈" 组合一旦创建，几乎永不释放——在 gfree 和活跃态之间反复流转，避免堆分配和 GC 压力。

### 5. 线程栈的实现（Linux）

```
线程栈在虚拟内存中的布局（x86-64）：

 高地址  ┌────────────────────┐
         │   已使用的栈帧       │  ← RSP 从高向低增长
         │   未使用（懒分配）    │  ← 虚拟空间已预留，物理页未映射
         │                    │
         │   剩余预留空间       │  ← 合计 8MB (ulimit -s)
 低地址  ├────────────────────┤
         │  Guard Page        │  ← 1 页（4KB），PROT_NONE
         └────────────────────┘     访问即 SIGSEGV
```

| 特性 | 机制 |
|------|------|
| **创建** | `clone()` 时指定 `void *child_stack` |
| **大小** | `pthread_attr_setstacksize()`，Linux 默认 8MB |
| **物理内存** | 懒分配（demand paging）——虚拟空间全部预留，物理页仅实际访问时映射 |
| **溢出检测** | Guard Page（PROT_NONE）→ SIGSEGV，进程终止 |
| **增长/收缩** | **不支持**运行时增长或收缩 |

### 6. 为什么线程栈不能像 goroutine 栈一样拷贝？

```
要拷贝线程栈，OS 内核需要：
  1. 暂停该线程
  2. 找到栈上所有指向栈内地址的指针
  3. 更新这些指针到新地址
  4. 搬栈
  5. 修改线程上下文中的 RSP/RBP

步骤 2 是死穴——内核不知道栈上哪些字节是指针、哪些是整数。
没有编译器提供的 liveness map。
```

### 7. 对比总表

| 维度 | Goroutine 栈 | OS 线程栈 (Linux) |
|------|-------------|-------------------|
| **初始大小** | 2 KB (`_StackMin`) | 8 MB（默认） |
| **最大大小** | 1 GB（`maxstack`） | = 初始大小（固定） |
| **增长策略** | 翻倍（2→4→8→16→...KB），按需触发 | 无 |
| **增长触发** | 编译器插桩 `CMP SP, stackguard0` | 无 |
| **收缩策略** | GC 时：使用量 < 1/4 → 缩小为一半，最小 2KB | 不支持 |
| **分配路径（热）** | P.mcache.stackcache → lock-free | 内核 `do_mmap` → 虚拟空间预留 |
| **溢出后果** | `fatal("stack overflow")` | SIGSEGV → 进程终止 |
| **拷贝/移动** | ✅ `copystack` + 编译器 liveness map | ❌ 内核无指针信息 |
| **物理内存** | 接近实际使用量（增长 + 收缩） | 按触及的页增长，但永不下降 |
| **10 万个** | ~200 MB 虚拟空间 | ~800 GB（不可行） |
| **创建速度** | ~100 ns（gfree 复用） | ~10-50 μs（`clone()` 系统调用） |
| **复用** | ✅ G+栈放 gfree / stackcache | ❌ 线程退出后栈回收，不可复用 |

### 8. 一张图：两种栈的一生

```
Goroutine 栈：
  2KB ──深调用──► 4KB ──更深──► 8KB ──更深──► 16KB
                                          │
                                GC 时使用量 < 4KB
                                          ▼
                                      8KB ──再降──► 4KB ──► 2KB
  始终接近"实际需要的尺寸"

线程栈：
  8MB ───────────────────────────────────────► 8MB
    │                                              │
    物理页按需分配，虚拟空间永远 8MB                   永不改变
    实际用 12KB，物理占 16KB（4 页）
    浪费的 ~8MB 虚拟空间不能给别的线程用
```

---

> **关联文件：** 本文的讨论已整合入 `gpm-scheduling.md` 的 §1.2、§3.2、§5 和 §8 中。本文是各节原始展开的独立存档，便于单独查阅。
