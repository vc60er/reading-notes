# Go GC 优化与 GC-friendly Cache / fasthttp 技术整理

## 1. 大型 Go 项目的 GC 优化建议

GC 优化不建议从调 `GOGC` 开始，而应优先：

> 减少分配 → 降低存活对象 → 控制堆增长 → 优化 GC 扫描 → 最后调整 GC 参数

主要目标包括：

- GC CPU 占用
- GC 周期频率
- STW / GC pause
- Heap 峰值
- RSS / 内存占用
- P99/P999 延迟
- 高并发吞吐量

### Allocation 与 Live Heap

GC 压力可以粗略理解为：

`GC 压力 ≈ 分配速度 × 存活对象数量 × 对象图复杂度`

需要区分：

1. 分配很多、对象很快死亡：主要是 allocation rate 高。
2. 大量对象长期存活：主要是 live heap、object graph 和 pointer 数量高。

第二种通常更需要关注数据结构和生命周期。

### Escape Analysis

可以使用：

```bash
go test -gcflags="-m"
```

检查对象为什么逃逸到 heap。

例如：

```go
func foo() *User {
    u := User{}
    return &u
}
```

如果 `u` 逃逸到 heap，就增加了 GC 管理的对象。

### 短生命周期对象

高 QPS 服务中，每个请求创建多个临时对象，很容易产生百万级 allocation/sec。

适合关注：

- Request / Response
- `[]byte`
- `bytes.Buffer`
- JSON 编解码对象
- 临时结构体
- 日志对象

### sync.Pool

适合复用：

- 生命周期短
- 可重置
- 大小相对稳定

的临时对象。

示例：

```go
var pool = sync.Pool{
    New: func() any {
        return new(bytes.Buffer)
    },
}
```

注意 `sync.Pool` 不是业务缓存，GC 时其中对象可能被清理。

对于大 buffer，还要避免偶发超大对象被长期放入 Pool，例如：

```go
if cap(buf) < maxPoolSize {
    pool.Put(buf)
}
```

### 减少 Pointer

GC 更关心对象中的 pointer，而不是单纯的数据大小。

例如：

```go
[]int64
```

和：

```go
[]*User
```

长度相同，GC 扫描成本可能差异很大。

大型项目应关注：

- `[]*T` vs `[]T`
- `map[string]*T` vs 更紧凑的数据布局
- 不必要的 pointer
- 过深的对象引用关系

### Map

例如：

```go
map[string]*User
```

可能形成：

```text
map
 ↓
string
 ↓
User pointer
 ↓
User 内部 pointer
```

大量 entry 时会形成庞大的 object graph。

### pprof

重点观察：

- `inuse_space`
- `inuse_objects`
- `alloc_space`
- `alloc_objects`

含义：

- `inuse_space`：当前仍存活的对象占用空间
- `inuse_objects`：当前存活对象数量
- `alloc_space`：运行以来累计分配空间
- `alloc_objects`：运行以来累计创建对象数量

例如：

`alloc_objects 很高 + inuse_objects 很低` 通常说明大量短生命周期对象。

### GOGC

提高 `GOGC` 通常意味着：

- GC 次数下降
- GC CPU 下降
- Heap 增长

本质是用内存换 CPU。

降低 `GOGC` 则通常相反。

因此 GOGC 不是简单的“GC 性能开关”，而是 CPU / Memory trade-off 参数。

### GOMEMLIMIT

Go 1.19+ 可使用：

```bash
GOMEMLIMIT=8GiB
```

它给 runtime 一个软内存目标。

Kubernetes 中不建议简单设置成 container memory limit，因为还需要给：

- goroutine stack
- runtime metadata
- mmap
- CGO
- 其他 native memory

留出空间。

---

# 2. Go GC 是否扫描数组、切片、string？

核心原则：

> GC 关心的是对象中是否存在 Go pointer，而不是普通数据本身。

### 数组

```go
[10000000]int64
```

虽然很大，但元素没有 pointer，不需要逐个检查。

而：

```go
[10000000]*User
```

包含大量 pointer，需要扫描这些 pointer。

### Slice

Slice 可以粗略理解为：

```go
type sliceHeader struct {
    Data *T
    Len  int
    Cap  int
}
```

因此 slice header 自己包含一个 data pointer。

但：

```go
[]int64
```

底层数组没有 pointer。

而：

```go
[]*User
```

底层数组有大量 pointer，因此 GC 需要扫描。

### string

string 可以粗略理解为：

```go
type stringHeader struct {
    Data *byte
    Len  int
}
```

GC 需要处理 string 的 data pointer，但字符串里的 byte 不是 Go pointer，因此不会逐字节扫描字符串内容。

### []string

`[]string` 底层保存大量 string header。

每个 string header 都有一个 data pointer，因此 `[]string` 比 `[]byte` 有更多 GC 扫描工作。

---

# 3. GC-friendly Cache 的核心技术

市面上的 GC-friendly Cache 并不是让 GC 算法本身变快，而是：

> 让 Cache 数据尽可能不表现为大量 pointer-rich Go objects。

核心技术包括：

1. `[]byte` / flat memory
2. pointer-free index
3. offset / handle 代替 Go pointer
4. Ring Buffer
5. 大块预分配
6. 对象复用
7. sharding
8. 序列化数据，减少 object graph

典型实现：

- BigCache
- FreeCache
- Ristretto

### BigCache

核心思路：

```text
map[uint64]uint32
       ↓
    offset
       ↓
     []byte
```

不是：

```go
map[string]*Entry
```

这样索引本身不包含大量 Go pointer，真正的数据集中放在 byte array 中。

### FreeCache

FreeCache 更激进：

```text
256 segments
+
每个 segment RingBuffer
+
index / offset
```

官方设计强调整个核心结构只有约 512 个主要 pointer。

大量 Cache entry 不作为独立 Go object 长期存在，而是存放在连续 byte buffer 中。

### Ring Buffer

例如：

```text
┌──────────────────────────────┐
│ Entry │ Entry │ Entry │ ... │
└──────────────────────────────┘
 ↑                         ↑
old                       new
```

空间满后覆盖旧数据，而不是不断创建和销毁大量 Go object。

### Offset / Handle

用：

```go
uint32
```

表示 entry 在 byte array 中的位置，而不是：

```go
*Entry
```

即：

> 用“逻辑 pointer”替代 Go pointer。

### 序列化成 []byte

普通：

```text
map → *Object → 子对象 → pointer graph
```

GC-friendly：

```text
serialize(Object)
       ↓
      []byte
```

代价是增加 CPU 和序列化/反序列化成本。

---

# 4. FreeCache 实现细节

可以概括为：

> 256 个分片 + 每个分片一个 RingBuffer + 连续索引数组 + 极少 Go pointer。

整体：

```text
FreeCache
│
├── Segment 0
│    ├── RingBuffer
│    ├── slotsData
│    └── lock
│
├── Segment 1
│    ├── RingBuffer
│    ├── slotsData
│    └── lock
│
└── Segment 255
```

Key：

```text
key
 ↓
hash
 ↓
segment
 ↓
slot
 ↓
offset
 ↓
RingBuffer
```

Entry 在 RingBuffer 中大致是：

```text
┌──────────────┬─────────┬───────────┐
│ Entry Header │   Key   │   Value   │
└──────────────┴─────────┴───────────┘
```

Header 中记录 key/value 长度、TTL、hash 等信息。

Get：

```text
Get(key)
  ↓
hash
  ↓
segment
  ↓
slot
  ↓
slotsData
  ↓
offset
  ↓
RingBuffer[offset]
  ↓
读取 Header
  ↓
校验 key
  ↓
检查 TTL
  ↓
返回 value
```

Set：

```text
Set(key,value)
  ↓
hash
  ↓
segment
  ↓
加锁
  ↓
查 index
  ↓
已有 entry？
  ├─ yes → 尝试复用原 value capacity
  └─ no  → append 到 RingBuffer
```

其关键代价是：增加一些 CPU、索引计算和内存访问复杂度，换取低 GC overhead 和有界内存。

---

# 5. fasthttp 中的 GC-friendly 技术

fasthttp 的核心目标与 FreeCache 不完全相同：

> FreeCache 更关注降低大量长期存活 Cache 数据的 GC 扫描压力；fasthttp 更关注降低高 QPS 请求路径上的 allocation rate。

主要技术：

### 1. RequestCtx / Request / Response 重用

模型：

```text
Request
 ↓
RequestCtx
 ↓
处理
 ↓
Reset
 ↓
下一次请求继续复用
```

减少大量 request-level object allocation。

### 2. 大量使用 []byte

避免不必要的：

```text
[]byte → string
```

从而减少 copy 和 allocation。

### 3. UnsafeString / UnsafeBytes

通过 unsafe 在某些场景下共享底层内存：

```text
[]byte
 ↑    ↑
 │    │
bytes string
```

避免转换时的数据复制。

代价是破坏部分类型安全 / 生命周期约束，必须非常谨慎。

### 4. sync.Pool

使用：

```text
Acquire*
   ↓
Pool.Get
   ↓
Reset
   ↓
使用
   ↓
Release*
   ↓
Pool.Put
```

复用 URI、Cookie 等临时对象。

### 5. Buffer reuse

大量使用：

```go
buf = buf[:0]
```

保留底层 capacity，重复利用已有内存。

### 6. Acquire / Release API

通过显式：

```go
AcquireXxx()
ReleaseXxx()
```

降低临时对象创建。

### 7. 避免 fmt 热路径

避免：

```go
fmt.Sprintf(...)
fmt.Fprintf(...)
```

改用更直接的 append 类 API，例如整数直接 append 到 byte buffer。

### 8. 自定义 Header / URI 数据结构

尽量直接在 byte buffer 上解析和操作 HTTP 数据，而不是构造：

```text
map[string][]string
+
大量 string
+
大量临时对象
```

### 9. 尽量原地解析

典型思想：

```text
network buffer
      ↓
直接解析
      ↓
slice / offset
      ↓
继续使用原始 buffer
```

而不是：

```text
network buffer
 ↓
复制
 ↓
string
 ↓
新的对象
 ↓
新的 slice
```

---

# 6. FreeCache vs fasthttp

可以这样理解：

| 方向 | FreeCache | fasthttp |
|---|---|---|
| 核心目标 | 降低 Cache 的 GC 扫描压力 | 降低请求路径 allocation |
| 大量使用 []byte | 是 | 是 |
| pointer-free 数据结构 | 核心 | 部分 |
| offset | 核心 | 常见 |
| RingBuffer | 核心 | 部分场景 |
| sync.Pool | 相对少 | 大量 |
| Object reuse | 有 | 核心 |
| unsafe bytes/string | 不是核心 | 重要优化手段 |
| zero allocation hot path | 目标之一 | 核心目标 |

最终可以抽象成：

```text
                    GC Friendly
                         │
           ┌─────────────┴─────────────┐
           ↓                           ↓
      少产生垃圾                    少产生 Pointer
           │                           │
           ↓                           ↓
  Pool / Object reuse              []byte
  Buffer reuse                     Flat memory
  Request reuse                    Offset
           │                           │
           └─────────────┬─────────────┘
                         ↓
                    GC 压力下降
```

最核心的一句话：

> **GC-friendly 的本质不是“让 GC 更努力”，而是让 GC 需要管理和扫描的 Go object / pointer 数量变少。**
