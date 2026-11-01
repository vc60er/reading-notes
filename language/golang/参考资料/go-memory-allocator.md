# Go 语言内存分配机制：源码级深度剖析

> 基于 Go 1.18 源码 (`src/runtime/`)，不涉及 GC 回收逻辑。

---

## 目录

1. [设计哲学与 TCMalloc 渊源](#1-设计哲学与-tcmalloc-渊源)
2. [虚拟内存布局：Arena](#2-虚拟内存布局arena)
3. [三级缓存架构总览](#3-三级缓存架构总览)
4. [Size Class：大小分级体系](#4-size-class大小分级体系)
5. [mspan：内存管理的基本单元](#5-mspan内存管理的基本单元)
6. [Page Allocator：页分配器](#6-page-allocator页分配器)
7. [mheap：全局堆管理器](#7-mheap全局堆管理器)
8. [mcentral：中心缓存层](#8-mcentral中心缓存层)
9. [mcache：Per-P 本地缓存](#9-mcacheper-p-本地缓存)
10. [Tiny Allocator：微小对象分配器](#10-tiny-allocator微小对象分配器)
11. [mallocgc：分配主函数全路径](#11-mallocgc分配主函数全路径)
12. [Large Object：大对象分配](#12-large-object大对象分配)
13. [refill 机制：缓存补货](#13-refill-机制缓存补货)
14. [Zeroing 策略：延迟清零](#14-zeroing-策略延迟清零)
15. [Stack Cache：栈内存的分配](#15-stack-cache栈内存的分配)
16. [fixalloc：固定大小非堆内存分配器](#16-fixalloc固定大小非堆内存分配器)
17. [persistentalloc：持久化内存分配器](#17-persistentalloc持久化内存分配器)
18. [内存统计体系](#18-内存统计体系)
19. [完整分配链路示例](#19-完整分配链路示例)

---

## 1. 设计哲学与 TCMalloc 渊源

Go 的内存分配器最初基于 Google 的 **TCMalloc**（Thread-Caching Malloc）设计，但经过多年演化已经产生了显著差异。其核心思想是：

- **多级缓存减少锁竞争**：Per-P → 全局中心 → 操作系统
- **大小分级（Size Class）**：将小对象的分配规格化到固定的几十种大小，避免碎片
- **Span 作为中间层**：在"页"和"对象"之间引入 span 抽象，解耦页管理与对象管理
- **延迟清零**：只在分配时清零，而不是释放时，利用时间局部性

核心判断逻辑从 `mallocgc` (`src/runtime/malloc.go:909`) 进入：

```
size == 0  → zerobase（零大小对象共享一个全局地址）
size <= 32KB → 小对象路径（可能走 tiny 或 size class）
size > 32KB  → 大对象路径（直接走 mheap 分配 span）
```

---

## 2. 虚拟内存布局：Arena

### 2.1 Arena 概念

Go 把整个堆的虚拟地址空间划分为等大的 **Arena**（`src/runtime/malloc.go:79-101`）：

| 平台 | Arena 大小 | 可寻址空间 |
|------|-----------|-----------|
| 64-bit Linux/Mac | **64 MB** | 48 位 |
| 64-bit Windows | **4 MB** | 48 位 |
| 32-bit | **4 MB** | 32 位 |
| iOS arm64 | **4 MB** | 33 位 |

```go
// src/runtime/malloc.go:254
heapArenaBytes = 1 << logHeapArenaBytes

// 每个 arena 的 bitmap 大小：
// src/runtime/malloc.go:262
heapArenaBitmapBytes = heapArenaBytes / (goarch.PtrSize * 8 / 2)
```

一个 64MB 的 arena 包含 `pagesPerArena = heapArenaBytes / pageSize = 64MB / 8KB = 8192` 页。

### 2.2 heapArena 元数据结构

每个 arena 有一个对应的 `heapArena` 元数据对象（`src/runtime/mheap.go:226-293`，存储在堆外）：

```go
// src/runtime/mheap.go:226
type heapArena struct {
    bitmap       [heapArenaBitmapBytes]byte  // 字级别的 pointer/scalar 位图
    spans        [pagesPerArena]*mspan        // 页号 → mspan 的映射
    pageInUse    [pagesPerArena / 8]uint8     // 哪些页的 span 处于使用状态
    pageMarks    [pagesPerArena / 8]uint8     // 哪些 span 上有被标记的对象
    pageSpecials [pagesPerArena / 8]uint8     // 哪些 span 有 finalizer
    zeroedBase   uintptr                      // 该 arena 中已确认清零的起始偏移
}
```

其中最重要的两个字段：

- **`spans`**：将虚拟地址映射到 `*mspan`。给定一个指针，可以快速找到它属于哪个 span（`spanOf()` → `src/runtime/mheap.go:627`）
- **`bitmap`**：记录每个字的类型信息（是否是指针），供 GC 扫描时使用

### 2.3 Arena 的两级索引

由于 48 位地址空间巨大，`mheap_.arenas` 采用两级数组（`src/runtime/mheap.go:154`）：

```go
// src/runtime/mheap.go:154
arenas [1 << arenaL1Bits]*[1 << arenaL2Bits]*heapArena
```

在多数 64-bit 平台上 `arenaL1Bits == 0`，退化为一级数组（约 32MB）。

地址到 arena 的转换（`src/runtime/mheap.go:559`）：

```go
func arenaIndex(p uintptr) arenaIdx {
    return arenaIdx((p - arenaBaseOffset) / heapArenaBytes)
}
```

`arenaBaseOffset` 在 amd64 上是 `0xffff800000000000`（处理符号扩展）。起始堆地址选择 `0x00c0<<32`，为了让指针值以 `0x00c0` 开头，在调试时容易识别，且不是合法 UTF-8 序列。

### 2.4 操作系统交互：四态模型

Go 将内存区域定义为四种状态（`src/runtime/malloc.go:358-424`）：

```
None → sysReserve → Reserved → sysMap → Prepared → sysUsed → Ready
```

- **None**：未保留
- **Reserved**：保留地址空间，访问会 fault，不计入进程内存
- **Prepared**：可用于高效转换为 Ready
- **Ready**：可以安全访问

对应的 OS 抽象函数：`sysAlloc`, `sysFree`, `sysReserve`, `sysMap`, `sysUsed`, `sysUnused`。

`mheap_.sysAlloc`（`src/runtime/malloc.go:636`）负责向 OS 申请新的 arena，并创建对应的 `heapArena` 元数据。

---

## 3. 三级缓存架构总览

```
                        ┌─────────────────────┐
                        │    应用程序           │
                        └──────────┬──────────┘
                                   ↓ new(T) / make()
                        ┌──────────────────────┐
                        │     mallocgc()       │  ← 分配主函数
                        └──────────┬───────────┘
                                   ↓
              ┌────────────────────┴────────────────────┐
              ↓ size <= 32KB?                           ↓ size > 32KB
    ┌─────────────────┐                        ┌─────────────────┐
    │  小对象 / tiny   │                        │    大对象路径     │
    └────────┬────────┘                        └────────┬────────┘
             ↓                                          ↓
    ┌─────────────────┐                        ┌─────────────────┐
    │   ① mcache      │  Per-P，无锁              │    mheap.alloc  │
    │   (per-P cache) │                           │    (跨过 mcache  │
    └────────┬────────┘                           │     和 mcentral) │
             ↓ 缓存 miss                          └─────────────────┘
    ┌─────────────────┐
    │   ② mcentral     │  加锁，按 size class 管理
    │   (central list) │
    └────────┬────────┘
             ↓ 缓存 miss
    ┌─────────────────┐
    │   ③ mheap        │  加锁，按 page 管理
    │   (page heap)    │
    └────────┬────────┘
             ↓ 堆不够
    ┌─────────────────┐
    │   ④ OS           │  mmap / VirtualAlloc
    └─────────────────┘
```

关键设计原则：

- **第①层无锁**（每个 P 拥有独立的 mcache）
- **第②层有锁**，但通过"一次取一个 span"摊薄锁开销
- **第③层有锁**，但操作次数极少
- **第④层**更是罕见

---

## 4. Size Class：大小分级体系

### 4.1 常量定义

```go
// src/runtime/sizeclasses.go:84-91
_MaxSmallSize   = 32768   // 小对象上限：32KB
smallSizeDiv    = 8       // ≤1024B 的查找粒度
smallSizeMax    = 1024    // 第一段查找表的上限
largeSizeDiv    = 128     // >1024B 的查找粒度
_NumSizeClasses = 68      // size class 总数
_PageShift      = 13      // 页大小 = 8KB
```

### 4.2 完整的 68 个 Size Class

```go
// src/runtime/sizeclasses.go:93
var class_to_size = [_NumSizeClasses]uint16{
    0, 8, 16, 24, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176,
    192, 208, 224, 240, 256, 288, 320, 352, 384, 416, 448, 480,
    512, 576, 640, 704, 768, 896, 1024, 1152, 1280, 1408, 1536,
    1792, 2048, 2304, 2688, 3072, 3200, 3456, 4096, 4864, 5376,
    6144, 6528, 6784, 6912, 8192, 9472, 9728, 10240, 10880,
    12288, 13568, 14336, 16384, 18432, 19072, 20480, 21760,
    24576, 27264, 28672, 32768,
}
```

设计目标：在"内碎片浪费"和"size class 数量"间取平衡。例如 class 43（3456 bytes）的 max waste 只有 8.83%。

### 4.3 从"请求大小"到"size class"的 O(1) 查找

使用两个查找表（`src/runtime/malloc.go:1074-1079`）：

```go
// 请求 size → 实际分配大小
func roundupsize(size uintptr) uintptr {
    if size < _MaxSmallSize {
        if size <= smallSizeMax-8 {
            return uintptr(class_to_size[
                size_to_class8[divRoundUp(size, smallSizeDiv)]])
        } else {
            return uintptr(class_to_size[
                size_to_class128[divRoundUp(size-smallSizeMax, largeSizeDiv)]])
        }
    }
    return alignUp(size, _PageSize)
}
```

- `size ≤ 1016`：查 `size_to_class8`（以 8 字节为步长，共 128 项）
- `1024 < size ≤ 32768`：查 `size_to_class128`（以 128 字节为步长，共 ~249 项）

### 4.4 spanClass 的"扫描"/"不扫描"双轨制

每个 size class 又分为两个 `spanClass`（`src/runtime/mheap.go:528-545`）：

```go
type spanClass uint8
const numSpanClasses = _NumSizeClasses << 1  // 68 * 2 = 136

func makeSpanClass(sizeclass uint8, noscan bool) spanClass {
    return spanClass(sizeclass<<1) | spanClass(bool2int(noscan))
}
```

- **scan**：对象包含指针，GC 需要扫描
- **noscan**：对象不含指针（如 `int`, `string` 的底层数据），GC 不需要扫描

将 scan/noscan 分开的好处：noscan 对象永远不会被 GC 扫描，减少了扫描工作量和写屏障开销。如果 `typ == nil || typ.ptrdata == 0`，则走 noscan 路径。

---

## 5. mspan：内存管理的基本单元

### 5.1 定义

`mspan` 是 Go 内存分配器中最核心的结构体（`src/runtime/mheap.go:381-462`）：

```go
type mspan struct {
    next *mspan          // 双向链表
    prev *mspan
    list *mSpanList

    startAddr uintptr    // span 起始地址
    npages    uintptr    // 包含多少页 (8KB 为单位)

    // 空闲对象管理
    freeindex  uintptr   // 从哪开始扫描空闲对象
    nelems     uintptr   // span 中共有多少个对象
    allocCache uint64    // allocBits 的补码缓存（用 ctz 指令快速查找空闲位）
    allocBits  *gcBits   // 分配位图
    gcmarkBits *gcBits   // GC 标记位图

    allocCount  uint16       // 已分配对象数
    spanclass   spanClass    // 大小 class + scan/noscan
    state       mSpanStateBox // mSpanDead/mSpanInUse/mSpanManual
    needzero    uint8        // 是否需要清零
    elemsize    uintptr      // 该 span 每个对象的大小
    limit       uintptr      // span 数据区的结束地址

    specials    *special     // finalizer 等特殊记录
}
```

### 5.2 bit 位：allocCache 的优化

`allocCache` 是 Go 分配器的一个关键性能优化（`src/runtime/malloc.go:849-865`）：

这是一个 64 位的值，存储的是 `allocBits` 的补码——0 表示空闲、1 表示已分配。使用 CPU 的 `CTZ`（Count Trailing Zeros）指令可以在单条指令中找到第一个空闲槽位：

```go
func nextFreeFast(s *mspan) gclinkptr {
    theBit := sys.Ctz64(s.allocCache)   // 找到第一个 0 位 = 第一个空闲对象
    if theBit < 64 {
        result := s.freeindex + uintptr(theBit)
        if result < s.nelems {
            s.allocCache >>= uint(theBit + 1)  // 消耗掉已分配的位
            s.freeindex = result + 1
            s.allocCount++
            return gclinkptr(result*s.elemsize + s.base())
        }
    }
    return 0  // 缓存失效，走慢路径
}
```

这是 fast path。当 allocCache 中找不到空闲槽位时，走 `nextFree()` 慢路径，它会到 `allocBits` 中寻找。

### 5.3 分配位图（allocBits）的更新

`freeindex` 和 `allocBits` 共同维护一个 span 的空闲/已分配状态。每次分配后：
- `freeindex` 前进
- `allocBits` 的对应位置 1
- `allocCache` 被移位

### 5.4 sweepgen：与 GC 的协调

```go
// src/runtime/mheap.go:444-451
// sweepgen == h->sweepgen - 2 → 需要 sweep
// sweepgen == h->sweepgen - 1 → 正在 sweep
// sweepgen == h->sweepgen     → 已 sweep，可用
// sweepgen == h->sweepgen + 1 → 在 sweep 开始前被缓存，仍需要 sweep
// sweepgen == h->sweepgen + 3 → 已 sweep 并被缓存
```

每次 GC 周期结束时 `sweepgen += 2`。

---

## 6. Page Allocator：页分配器

`mheap.pages` 是 Go 1.16+ 引入的新页分配器（`src/runtime/mheap.go:66`），实现了地址有序的 first-fit 策略。

### 6.1 页缓存（pageCache）

每个 P 还有一个 Per-P 的页缓存 `pcache`（`src/runtime/mheap.go:1134-1153`）：

```go
// 小量页申请（< pageCachePages/4 = 16 页）优先走 page cache
if !needPhysPageAlign && pp != nil && npages < pageCachePages/4 {
    c := &pp.pcache
    if c.empty() {
        lock(&h.lock)
        *c = h.pages.allocToCache()  // 从全局填充
        unlock(&h.lock)
    }
    base, scav = c.alloc(npages)    // 从本地分配
}
```

页缓存避免了小量页申请时每次都去争用 `mheap.lock`。

### 6.2 堆增长

当现有内存不够时，调用 `mheap.grow`（`src/runtime/mheap.go:1339`）：

```go
func (h *mheap) grow(npage uintptr) (uintptr, bool) {
    ask := alignUp(npage, pallocChunkPages) * pageSize  // 至少 1MB
    // 尝试在 curArena 中拓展
    // 如果不够，调用 h.sysAlloc 申请新的 arena
}
```

### 6.3 span 结构体本身的分配与缓存

在 `allocSpan` 中，获取 `mspan` 结构体本身也经过缓存（`src/runtime/mheap.go:1032-1100`）：

```go
// 每个 P 有 mspancache（固定大小的 mspan 指针数组）
func (h *mheap) tryAllocMSpan() *mspan   // 从 P 缓存取（无锁）
func (h *mheap) allocMSpanLocked() *mspan // 缓存不命中时加锁分配
func (h *mheap) freeMSpanLocked(s *mspan) // 释放回 P 缓存
```

`mspan` 结构体本身由 `fixalloc` 管理（`spanalloc`），不在 GC 堆上。

---

## 7. mheap：全局堆管理器

`mheap` 是全局唯一的堆管理器（`src/runtime/mheap.go:62-218`）：

```go
//go:notinheap
type mheap struct {
    lock       mutex
    pages      pageAlloc     // 页分配器
    sweepgen   uint32

    allspans   []*mspan      // 所有 span 的切片

    // 两级 arena 索引
    arenas     [1 << arenaL1Bits]*[1 << arenaL2Bits]*heapArena
    allArenas  []arenaIdx    // 所有已映射的 arena 索引

    curArena   struct { base, end uintptr }  // 当前正在增长的 arena

    // 68 * 2 = 136 个 central，按 spanClass 索引，cache line 对齐
    central [numSpanClasses] struct {
        mcentral mcentral
        pad      [cpu.CacheLinePadSize - ...]byte  // 伪共享防护
    }

    // 固定大小分配器（非 GC 堆上使用）
    spanalloc             fixalloc  // mspan 结构体
    cachealloc            fixalloc  // mcache 结构体
    specialfinalizeralloc fixalloc  // finalizer 记录
    arenaHintAlloc        fixalloc  // arena 扩展提示
}

var mheap_ mheap  // 全局单例
```

`central` 数组中的 padding 确保每个 `mcentral` 独享 cache line，防止 false sharing。

---

## 8. mcentral：中心缓存层

### 8.1 结构

`mcentral` 管理同一个 size class 的所有 span（`src/runtime/mcentral.go:20-43`）：

```go
type mcentral struct {
    spanclass spanClass
    partial   [2]spanSet   // 有空闲对象的 span
    full      [2]spanSet   // 已满的 span
}
```

这里每个都是长度为 2 的数组，采用**双缓冲（double buffering）**设计：
- `partial[sweepgen/2%2]` → 已 sweep 的
- `partial[1-sweepgen/2%2]` → 未 sweep 的

每次 GC 周期 sweepgen 增加，两个角色互换。

### 8.2 cacheSpan：为 mcache 提供 span

这是 mcentral 最核心的方法（`src/runtime/mcentral.go:79-188`）：

```
1. 从 partialSwept 弹出 → 如果成功，直接使用
2. 从 partialUnswept 弹出 → sweep 后使用（最多尝试 100 次）
3. 从 fullUnswept 弹出 → sweep，有空闲就使用，否则放回 fullSwept
4. 以上都失败 → 调用 c.grow() 从 mheap 申请新 span
```

100 次的预算限制（spanBudget = 100）保证了不会因为长时间寻找空闲 span 而造成分配延迟过大。

### 8.3 uncacheSpan：从 mcache 退回

当 mcache 中的 span 已满或 P 被释放时，span 被退回 mcentral（`src/runtime/mcentral.go:194-237`）：

- **stale span**（在 sweep 开始前就被缓存）：自己 sweep
- **正常 span**：根据是否有空闲，放入 partialSwept 或 fullSwept

---

## 9. mcache：Per-P 本地缓存

### 9.1 结构

每个 P 都持有一个 `mcache`（`src/runtime/mcache.go:20-53`）：

```go
type mcache struct {
    nextSample  uintptr   // heap profiling 采样点
    scanAlloc   uintptr   // 已分配的可扫描字节数

    // Tiny 分配器
    tiny        uintptr   // 当前 tiny 块的起始地址
    tinyoffset  uintptr   // 当前 tiny 块的已用偏移
    tinyAllocs  uintptr   // tiny 分配计数（延迟刷新）

    // 每个 spanClass 一个活跃 span（共 136 个槽位）
    alloc       [numSpanClasses]*mspan

    // 栈缓存
    stackcache  [_NumStackOrders]stackfreelist

    flushGen    uint32    // 上次刷新时的 sweepgen
}
```

`mcache` 从非 GC 内存分配（`go:notinheap`），这意味着其中的堆指针（如 `tiny`、`alloc` 中的 span）需要特殊处理。

### 9.2 无锁分配

`alloc[spc]` 是每个 span class 的"活跃 span"。该 P 的所有小对象分配直接从这里的 span 中取对象，完全无锁。只有当这个 span 满了，才调用 `refill()` 触发带锁的 mcentral 交互。

### 9.3 生命周期

- **创建**：`allocmcache()`（`src/runtime/mcache.go:84`）在 P 被创建时调用，初始化所有 `alloc[i] = &emptymspan`
- **刷新**：`releaseAll()` 将活跃 span 退回 mcentral
- **释放**：`freemcache()` 清理后由 `cachealloc` fixalloc 回收

---

## 10. Tiny Allocator：微小对象分配器

### 10.1 设计动机

t ≤ 16 字节且不含指针的小对象（如小字符串、逃逸的单个字段），如果每个都单独分配，内存浪费严重。Tiny allocator 将多个这种对象合并到一个 16 字节的块中（`src/runtime/malloc.go:999-1071`）。

常量定义（`src/runtime/malloc.go:135-136`）：
```go
_TinySize      = 16
_TinySizeClass = int8(2)     // 对应 size class 2 → 16 字节
```

### 10.2 分配流程

```
1. 检查 size < 16 && noscan（不含指针）
2. 在 c.tiny 块中尝试放置：
   - 根据 size 的幂等对齐要求调整 off
   - 如果 off + size <= 16 则直接返回 c.tiny + off
   - 更新 c.tinyoffset = off + size
3. 放不下或 c.tiny == 0：
   - 从 tinySpanClass 分配一个新的 16 字节块
   - 将新对象放入，更新 c.tiny 和 c.tinyoffset
```

### 10.3 内存浪费上限

官方注释（`src/runtime/malloc.go:1009-1016`）解释了 16 字节这个选择的考量：

- 8 字节：无浪费，但合并机会少
- 16 字节（当前）：最多 2x 浪费
- 32 字节：更多合并机会，但最多 4x 浪费

在 JSON benchmark 中，tiny allocator 减少了约 12% 的分配次数和约 20% 的堆大小。

---

## 11. mallocgc：分配主函数全路径

`mallocgc`（`src/runtime/malloc.go:909-1226`）是所有堆分配的唯一入口。`new(T)` 和 `make([]T, n)` 最终都调用它。

### 11.1 前置处理

```go
func mallocgc(size uintptr, typ *_type, needzero bool) unsafe.Pointer {
    if size == 0 {
        return unsafe.Pointer(&zerobase)   // 所有 0 大小对象共享一个地址
    }

    // GC assist：如果当前 G 有 GC 债务，先帮助 GC 做标记工作
    if assistG.gcAssistBytes < 0 {
        gcAssistAlloc(assistG)
    }

    // 获取 M（禁止 GC 抢占）
    mp := acquirem()
    mp.mallocing = 1

    // 获取 P 的 mcache
    c := getMCache(mp)

    noscan := typ == nil || typ.ptrdata == 0   // 不含指针？
```

### 11.2 三条分配路径

```
        size <= maxSmallSize (32768)?
               /              \
            YES                NO
           /                      \
    noscan && size < 16?       大对象路径
      /           \            mcache.allocLarge()
    YES            NO          直接走 mheap.alloc()
     |              |
Tiny路径     Size Class路径
c.tiny      size_to_class8/128
(合并到     c.alloc[spc]
 16B块)     nextFreeFast/nextFree
```

### 11.3 Fast Path：nextFreeFast

```go
v := nextFreeFast(span)    // CTZ 指令定位空闲位
if v == 0 {
    v, span, shouldhelpgc = c.nextFree(spc)  // 慢路径
}
```

`nextFreeFast` 利用 `allocCache` 和 CPU 的 `CTZ` 指令在几个周期内定位空闲对象，是高性能分配的核心。

### 11.4 慢路径：nextFree

`nextFree`（`src/runtime/malloc.go:876`）当 fast path 失败时调用：

```go
func (c *mcache) nextFree(spc spanClass) (v gclinkptr, s *mspan, shouldhelpgc bool) {
    s = c.alloc[spc]
    freeIndex := s.nextFreeIndex()
    if freeIndex == s.nelems {
        // Span 已满，从 mcentral 获取新 span
        c.refill(spc)
        s = c.alloc[spc]
        freeIndex = s.nextFreeIndex()
    }
    v = gclinkptr(freeIndex*s.elemsize + s.base())
    s.allocCount++
    return
}
```

### 11.5 分配后处理

分配完成后，`mallocgc` 还会：

1. 如果对象含指针，设置 heap bitmap（`heapBitsSetType`）
2. 执行 publication barrier（确保在对象对外可见前，初始化已经完成）
3. 如果在 GC 阶段，将对象标黑（`gcmarknewobject`）
4. 处理 race/memory sanitizer 检测
5. Heap profiling 采样
6. 如果触发了 GC 阈值，启动 GC（`gcStart`）

---

## 12. Large Object：大对象分配

当 `size > 32768`（32KB + 1），走大对象路径（`src/runtime/malloc.go:1092-1113`）：

```go
} else {
    span = c.allocLarge(size, noscan)   // 计算所需页数，直接从 mheap 分配
    span.freeindex = 1
    span.allocCount = 1                 // 整个 span 只有这一个对象
    x = unsafe.Pointer(span.base())
}
```

`allocLarge`（`src/runtime/mcache.go:198-231`）：

```go
func (c *mcache) allocLarge(size uintptr, noscan bool) *mspan {
    npages := size >> _PageShift
    if size&_PageMask != 0 {
        npages++
    }
    deductSweepCredit(npages*_PageSize, npages)
    spc := makeSpanClass(0, noscan)    // size class 0 = "大对象"
    s := mheap_.alloc(npages, spc)
    // ...
    s.limit = s.base() + size           // 实际只用 size 这么多
}
```

关键点：
- 不经过 mcache 的 alloc 数组，也不经过 mcentral
- `elemsize = nbytes`（整个 span 的大小），但 `limit` 设为 `base + size`（实际请求大小）
- 一个 span 只包含一个大对象
- 仍然会被记录到 mcentral 的 fullSwept 列表中，供后台 sweep 使用

---

## 13. refill 机制：缓存补货

当 mcache 中的某类 span 满了，调用 `refill`（`src/runtime/mcache.go:146-195`）：

```go
func (c *mcache) refill(spc spanClass) {
    // 1. 把已满的 span 退回 mcentral
    s := c.alloc[spc]
    mheap_.central[spc].mcentral.uncacheSpan(s)

    // 2. 从 mcentral 获取有空闲的 span
    s = mheap_.central[spc].mcentral.cacheSpan()

    // 3. 标记此 span 已被缓存
    s.sweepgen = mheap_.sweepgen + 3

    // 4. 更新统计
    // ...

    // 5. 将新 span 挂到 mcache 上
    c.alloc[spc] = s
}
```

`sweepgen = mheap_.sweepgen + 3` 的特殊标记：表示这个 span 已经被 sweep 过并且当前被缓存在 mcache 中。下次 GC 循环时，其他 span 的 sweepgen 前进了两步（sweepgen += 2），这个标记会退化为 `sweepgen + 1`（指示这是一个"stale cached span"）。

---

## 14. Zeroing 策略：延迟清零

Go 采用延迟清零（`src/runtime/malloc.go:65-75`，`src/runtime/mheap.go:961-1018`）：

```go
// mspan.needzero == 1 → 分配对象时需要 memclr
// mspan.needzero == 0 → 对象槽位已经清零
```

`allocNeedsZero()` 通过 arena 的 `zeroedBase` 字段判断新分配的页是否已经清零过：

```go
func (h *mheap) allocNeedsZero(base, npage uintptr) (needZero bool) {
    for npage > 0 {
        zeroedBase := atomic.Loaduintptr(&ha.zeroedBase)
        arenaBase := base % heapArenaBytes
        if arenaBase < zeroedBase {
            needZero = true    // 这部分还没有被清零过
        }
        // CAS 更新 zeroedBase
    }
}
```

这保证了一个页在第一次被使用时只清零一次。

在 `mallocgc` 中（`src/runtime/malloc.go:1101-1112`），大对象走 `delayedZeroing` 路径——先返回未清零的内存给用户，稍后在可抢占上下文中再清零（利用时间局部性：用户大概率马上会写这些内存）。

---

## 15. Stack Cache：栈内存的分配

每个 mcache 还有栈缓存（`src/runtime/mcache.go:46`，`src/runtime/malloc.go:141-155`）：

```go
_StackCacheSize = 32 * 1024  // 每 P 32KB 栈缓存

// 缓存的栈大小：2KB, 4KB, 8KB, 16KB（取决于平台）
_NumStackOrders = 4 - goarch.PtrSize/4*goos.IsWindows - 1*goos.IsPlan9
```

每个 size order 有一个 `stackfreelist`——指向空闲栈的单链表。小栈从 P 的缓存中直接取，大于 16KB 的栈直接从 mheap 分配。

---

## 16. fixalloc：固定大小非堆内存分配器

`fixalloc` 是一个固定大小对象的空闲链表分配器（在 `src/runtime/mheap.go:215` 使用），用于管理**分配器自身的数据结构**：

```go
h.spanalloc.init(...)     // 分配 mspan 结构体
h.cachealloc.init(...)    // 分配 mcache 结构体
h.specialfinalizeralloc.init(...)  // 分配 finalizer 记录
h.arenaHintAlloc.init(...)  // 分配 arena 增长提示
```

这些对象不在 GC 堆上（`go:notinheap`），由 `fixalloc` 独立管理。每次池子空了，就从 OS 申请一个 16KB chunk（`_FixAllocChunk = 16KB`）。

`mspan` 在通过 `spanalloc` 分配时**不进行清零**。这是因为 sweep 过程中可能并发地检查一个 span，这个 span 被释放后又被重新分配，需要保留它的 `sweepgen` 字段。

---

## 17. persistentalloc：持久化内存分配器

`persistentalloc`（`src/runtime/malloc.go:1393-1467`）用于分配函数/类型/debug 相关的持久数据，这些内存从不释放，也不被 GC 管理。

它使用了 Per-P 的 `palloc` 线性分配器来减少锁竞争：

```go
type persistentAlloc struct {
    base *notInHeap
    off  uintptr
}
```

每次申请 256KB chunk，从 chunk 中线性分配。如果当前 P 没有或正在 bootstrapping，回退到全局的 `globalAlloc.persistentAlloc`。

---

## 18. 内存统计体系

Go 的内存统计有两层：

### 18.1 全局统计（mstats）

```go
// src/runtime/mstats.go:22-102
type mstats struct {
    alloc       uint64  // 分配且未释放
    total_alloc uint64  // 累计分配（含已释放）
    sys         uint64  // 从 OS 获得的内存总量
    nmalloc     uint64  // 分配次数
    nfree       uint64  // 释放次数

    // 堆统计（由 mheap 管理）
    heap_sys      sysMemStat  // 虚拟地址空间
    heap_inuse    uint64      // 使用中 span 的字节数
    heap_released uint64      // 还给 OS 的物理内存

    // 按 size class 的分配/释放计数
    by_size [_NumSizeClasses] struct {
        size    uint32
        nmalloc uint64
        nfree   uint64
    }
}
```

### 18.2 一致性统计（consistentHeapStats）

为了在多 P 并发场景下高效且正确地更新统计，Go 使用 `consistentHeapStats`（`src/runtime/mstats.go:748-783`）：

```
                    ┌──────────────────────┐
                    │ consistentHeapStats   │
                    │                       │
                    │ stats[0..2]  ←  P0   │
                    │ (ring buffer)  ←  P1  │
                    │ gen (0/1/2)    ←  ...  │
                    └──────────────────────┘
```

每个 P 在 `acquire()` 时获取 `stats[gen]` 的写入权，通过 `statsSeq`（奇数=正在写，偶数=未写）进行并发控制。`read()` 通过旋转 `gen` 值实现快照。

### 18.3 对外暴露：runtime.MemStats

`ReadMemStats` 将所有内部统计聚合到 `runtime.MemStats` 暴露给用户：

- `HeapAlloc`, `HeapSys`, `HeapIdle`, `HeapInuse`, `HeapReleased`, `HeapObjects`
- `StackInuse`, `StackSys`
- `MSpanInuse`, `MCacheInuse`（分配器元数据本身的开销）
- `GCSys`, `OtherSys`

---

## 19. 完整分配链路示例

以下追踪"分配一个 20 字节（含指针）的对象"的完整调用链：

```
new(MyStruct{...})                          // 用户代码
  └─ newobject(typ)                         // src/runtime/malloc.go:1258
       └─ mallocgc(typ.size=20, typ, true)  // src/runtime/malloc.go:909
            │
            ├─ size == 0? NO
            ├─ acquirem()                   // 防止 GC 抢占
            ├─ getMCache(mp)                // 取当前 P 的 mcache
            ├─ noscan == false (typ.ptrdata > 0)
            │
            ├─ size <= 32768? YES → 走小对象路径
            │    ├─ size <= 1016? YES
            │    ├─ roundupsize: 20 → size_to_class8[divRoundUp(20,8)]
            │    │                              = size_to_class8[3] = class 3
            │    ├─ class_to_size[3] = 24      → 实际分配 24 字节
            │    ├─ makeSpanClass(3, false)    → spanClass = 3<<1 | 0 = 6
            │    ├─ span = c.alloc[6]           → 取该 size class 的活跃 span
            │    ├─ nextFreeFast(span)          → CTZ 找空闲位
            │    │    └─ 有空闲 → 直接用
            │    │    └─ 无空闲 → c.nextFree(6)
            │    │         ├─ span 满了 → c.refill(6)
            │    │         │    ├─ uncacheSpan: 退回满 span 到 mcentral.fullSwept
            │    │         │    └─ mcentral.cacheSpan()
            │    │         │         ├─ 从 partialSwept 弹出 → 命中！
            │    │         │         └─ 如果失败 → 尝试 sweep unswept spans
            │    │         │         └─ 如果还失败 → mheap.alloc(npages, spc)
            │    │         │              └─ 从 pcache 或 pages 分配页
            │    │         │              └─ 如果不够 → mheap.grow → sysAlloc → mmap
            │    │         └─ 分配 & 返回
            │    └─ x = bool(span.base() + freeIndex*24)
            │
            ├─ heapBitsSetType(x, 24, 20, typ)  → 设置 bitmap（含指针信息）
            ├─ publicationBarrier()             → 内存屏障
            ├─ gcmarknewobject() (if gcphase)   → GC 中分配标黑
            ├─ releasem(mp)
            └─ return x
```

对于大对象（e.g., `make([]byte, 64KB)`）：

```
mallocgc(65536, ...)
  ├─ size > 32768 → 大对象路径
  ├─ mcache.allocLarge(65536, noscan)
  │    ├─ npages = 65536/8192 = 8页
  │    └─ mheap.alloc(8, makeSpanClass(0, true))
  │         ├─ tryAllocMSpan()    → 从 P 的 mspancache 拿 span 结构体
  │         ├─ pp.pcache.alloc(8) → 从 P 的 pcache 拿 8 页
  │         └─ 若不行 → lock → h.pages.alloc → 若不够 → h.grow
  ├─ span.elemsize = 8*8192 = 65536
  ├─ span.nelems = 1
  ├─ span.limit = base + 65536
  └─ return base
```

---

## 附录：关键文件索引

| 文件 | 内容 |
|------|------|
| `src/runtime/malloc.go` | 分配主函数 `mallocgc`、tiny allocator、arena 管理、persistent allocator |
| `src/runtime/mheap.go` | `mheap`、`mspan`、`heapArena` 定义，页分配器封装，堆增长逻辑 |
| `src/runtime/mcache.go` | `mcache` 定义，refill、allocLarge、releaseAll |
| `src/runtime/mcentral.go` | `mcentral` 定义，cacheSpan、uncacheSpan、grow |
| `src/runtime/sizeclasses.go` | 68 个 size class 表（自动生成） |
| `src/runtime/mstats.go` | `MemStats`、一致性统计、统计聚合 |
| `src/runtime/mbitmap.go` | Heap bitmap 操作（pointer/scalar 标记） |
| `src/runtime/mfixalloc.go` | `fixalloc` 实现 |

---

> **本文基于 Go 1.18 源码撰写，文件路径和行号以该版本为准。Go 1.16+ 引入了新的 page allocator；Go 1.19+ 和 1.22+ 有进一步的改进，但整体架构保持不变。**
