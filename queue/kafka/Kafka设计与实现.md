# Kafka 的设计与实现：从整体架构到核心源码机制

理解 Kafka，最重要的不是记住它有哪些组件，而是理解它为什么能同时实现**高吞吐、持久化、水平扩展和消费进度管理**。

Kafka 的核心设计思想可以概括为：

1. **存储层：** 把消息组织成分区日志，以追加写为主，利用顺序 I/O、操作系统页缓存和批量处理提升吞吐。
2. **分布式层：** 把 Topic 划分为多个 Partition，分布到不同 Broker，通过副本复制实现容错。
3. **消费层：** Broker 不主动推送消息，而是由 Consumer 拉取；消费进度由 Consumer Group 管理。
4. **可靠性层：** 通过副本同步、ISR、生产者确认机制和消费者位移管理，在性能、持久性与可用性之间进行权衡。
5. **性能层：** 批量发送、批量拉取、零拷贝、页缓存和顺序追加共同构成高吞吐的基础。

下面按照“整体架构 → 存储实现 → 生产与消费 → 副本复制 → 高可用 → 性能优化”的顺序展开。

## 一、先建立 Kafka 的整体模型

![All things Kafka - DEV Community](https://images.openai.com/static-rsc-4/92L3QN3H6f8enRvNIj6Szdt6G_QnZGxctxCIrVDt52fWIezWGQzNyf_h7_RbHIhCLTWBA3bqZZ50Y9Mr5izeDowauwu9CTmwWdbu9Pfj6ojajgaTTBqBrxN175VzZKucAiwzElm-y11hkOrU1ywQ8y16udLHcGGBAOp7cKnzdECcM-sjzf8nBIWsV_Z4cmIq?purpose=fullsize)

Kafka 可以分成四个相互配合的子系统：

1. **消息生产层：** Producer 将消息序列化、分配分区、批量发送，并根据 `acks` 决定需要等待何种确认。
2. **Broker 与日志存储层：** Broker 接收消息并追加到 Partition 日志；日志按照 Segment 分段，索引辅助定位消息。
3. **副本与集群控制层：** Leader 处理分区的主要读写请求，Follower 复制日志；Controller 管理分区领导者、Broker 状态等集群元数据。
4. **消费层：** Consumer 按 Offset 拉取消息；Consumer Group 将分区分配给组内消费者，并维护消费进度。

### 1.1 核心对象之间的关系

| 概念 | 本质 | 解决的问题 |
|---|---|---|
| Topic | 逻辑消息分类 | 消息如何分类 |
| Partition | 有序、可追加的日志 | 并行处理与水平扩展 |
| Segment | Partition 日志的物理分段 | 控制文件大小、清理与索引 |
| Offset | 分区内的逻辑位置 | 消息定位与消费进度 |
| Broker | Kafka 服务节点 | 存储和处理请求 |
| Replica | Partition 的副本 | 故障恢复与数据冗余 |
| Consumer Group | 一组协同消费的消费者 | 并行消费与负载分配 |
| Controller | 集群元数据协调者 | Leader 选举与集群状态管理 |

这里有一个非常重要的关系：**Topic 是逻辑概念，Partition 是并行与有序性的基本单位，Segment 是日志在磁盘上的物理组织单位。**

例如，一个 Topic 有 12 个 Partition，分布在 3 台 Broker 上，每台 Broker 承载 4 个 Leader Partition。这样就能让 3 台机器并行处理不同分区的消息。但是，同一个 Partition 内的消息仍然按照日志顺序排列；Kafka 并不承诺多个 Partition 之间存在全局顺序。

## 二、为什么 Kafka 选择日志模型，而不是传统消息队列模型？

这是理解 Kafka 所有后续设计的起点。

传统消息队列常见的思路是：消息投递给消费者，确认消费后再删除或转移消息状态。Kafka 则把消息看作一个持续增长的日志，消费者只维护自己的读取位置。

| 传统队列模型 | Kafka 日志模型 |
|---|---|
| 消息 → 投递 → ACK → 删除或确认 | 追加消息 → 保留日志 → 多个消费者各自读取 |
| 重点是管理消息的投递状态 | 重点是高效存储、读取和管理消费位置 |

例如：

```text
Partition 0:
Offset 0 → 用户注册
Offset 1 → 用户登录
Offset 2 → 用户下单
Offset 3 → 用户支付
Offset 4 → 用户退款
```

不同消费者可以独立读取：

```text
订单服务：       offset = 3
数据分析服务：   offset = 1
风控服务：       offset = 4
```

它们读取的是同一份逻辑日志，但进度相互独立。

这带来三个重要结果：

1. 同一条消息可以被多个业务重复读取，而不必为每个消费者复制一份消息。
2. 消费者故障后，可以从已提交的 Offset 恢复。
3. Kafka 不需要为每条消息维护所有消费者的投递、确认和删除状态，消费进度主要通过每个分区的 Offset 表示。

代价是：**Kafka 不会因为某个消费者读过消息，就立即删除这条消息。** 消息的保留由时间、空间和压缩策略决定。

这也是 Kafka 特别适合日志采集、事件流、数据管道和流式计算的原因。

## 三、存储引擎：Kafka 如何组织磁盘上的数据？

这是 Kafka 高吞吐设计最值得深入理解的部分。

### 3.1 Partition 的物理结构

**一个 Partition 通常对应 Broker 磁盘上的一个日志目录**。日志并非无限写入同一个文件，而是由多个 Segment 组成。

```text
Topic: orders / Partition: 0
│
├── Segment A：已关闭
│   ├── 00000000000000000000.log
│   └── 对应索引：.index / .timeindex
│       Offset 0、1、2 …
│
├── Segment B：已关闭
│   ├── 00000000000000000003.log
│   └── Offset 3、4、5 …
│
└── Segment C：当前活跃日志
    ├── 00000000000000000006.log
    └── Offset 6、7、8 … 持续追加
```

这里有两个关键设计。

**第一，Segment 文件名通常使用该段首条日志记录的基础 Offset。** 例如 `00000000000000000006.log` 表示这个 Segment 从基础 Offset 6 开始。它不意味着文件中每条记录都占用一个固定大小的 Offset 空间。

**第二，通常只有当前活跃 Segment 接受新的日志追加。** 当活跃 Segment 达到大小限制或满足滚动条件时，Kafka 会关闭当前段，创建新的 Segment。这样可以把文件清理、索引构建和历史日志管理限制在较小的物理单元内。

具体阈值由配置和版本实现决定，例如 `log.segment.bytes`、`log.roll.ms`。

### 3.2 为什么采用追加写？

假设每秒有大量消息进入系统。如果采用随机更新模型，可能需要频繁定位不同磁盘位置，产生随机 I/O。Kafka 则尽量将写入转换成：

```text
收到一批消息
    ↓
按顺序追加到当前日志文件
    ↓
操作系统将修改反映到页缓存
    ↓
后台或系统机制将脏页写入持久化存储
```

追加写的主要优势：

- 减少随机寻址，适合机械硬盘和 SSD。
- 可以把多条消息合并成较大的连续写入。
- 减少复杂的原地更新和空间分配操作。
- 更容易利用操作系统页缓存。

需要注意，**顺序追加不等于每条消息都同步落盘**。Kafka 通常依靠操作系统页缓存来提升性能，再结合副本复制、刷盘策略和故障恢复机制实现所需的可靠性。

### 3.3 `.log`、`.index`、`.timeindex` 分别是什么？

| 文件 | 用途 |
|---|---|
| `.log` | 存储实际的消息记录和批次 |
| `.index` | Offset 索引，辅助根据 Offset 定位日志位置 |
| `.timeindex` | 时间索引，辅助按时间戳查找消息位置 |
| `.txnindex` | 某些版本和场景下用于事务相关索引信息 |

索引并不是把每条消息的 Offset 和物理位置都完整存下来。Kafka 使用稀疏索引降低索引占用。

```text
Offset Index:
Offset 0    → 文件位置 0
Offset 100  → 文件位置 8192
Offset 200  → 文件位置 16384
```

当消费者请求 Offset 157 时，Kafka 可以：

1. 找到 Offset 157 所属的 Segment。
2. 在稀疏索引中寻找不大于 157 的最近索引项。
3. 从对应文件位置向后扫描，定位实际的消息批次。

所以读取过程是**分段定位 + 稀疏索引定位 + 局部扫描**。

### 3.4 Offset 到底是什么？

Offset 是分区内的逻辑位置，不是文件字节偏移量。例如 `Offset = 100` 并不意味着消息位于文件第 100 个字节。

Kafka 需要区分：

- **Offset**：日志中的逻辑位置。
- **File Position**：消息批次在文件中的物理字节位置。
- **LEO（Log End Offset）**：日志末尾的下一个逻辑 Offset。
- **Consumer Position**：消费者下一次准备读取的位置。

现代 Kafka 使用 Record Batch 存储记录，压缩批次和事务控制记录等因素意味着，不能简单地假定每条应用消息都对应一个独立、连续且一定可见的 Offset。日志压缩也可能让旧 Offset 对应的记录被清理，但 Offset 本身不会因此重新编号。

### 3.5 Kafka 为什么使用页缓存，而不是自己管理全部内存？

Kafka 的核心存储路径大量利用操作系统的 Page Cache。

```text
Producer
   ↓
Broker 写日志
   ↓
操作系统页缓存（内存）
   ↓
磁盘持久化
```

消费时：

```text
磁盘 / 页缓存
       ↓
Broker 读取日志
       ↓
网络发送给 Consumer
```

**如果消费者追赶得上生产速度，近期日志很可能已经在页缓存中，读取就不需要每次访问磁盘**。这使 Kafka 可以把内存用于缓存热点日志，而不必在 JVM 堆内维护一套完整的消息缓存。

页缓存命中率会受到内存大小、工作集、读取模式和系统压力影响，并不是所有消费请求都一定命中内存。

### 3.6 零拷贝：为什么 Kafka 的消费路径很快？

普通的数据传输路径可能涉及：

```text
磁盘
 ↓
内核页缓存
 ↓ 拷贝
用户态缓冲区
 ↓ 拷贝
内核 Socket 缓冲区
 ↓
网卡
```

对于符合条件的文件传输路径，Kafka 可以利用 Linux `sendfile` 等机制，让数据从页缓存更直接地进入网络发送路径，避免将完整消息反复拷贝到用户态。

```text
磁盘
 ↓
内核页缓存
 ↓ sendfile 等机制
Socket / 网络发送路径
 ↓
网卡
```

这通常被称为零拷贝优化。但零拷贝并不是任何情况下都绝对没有数据复制。TLS 加密、压缩、消息格式处理、事务过滤或其他路径条件，都可能改变实际的数据传输方式。

Kafka 的高性能并非来自零拷贝单一技术，而是由**批量处理、顺序追加、页缓存、稀疏索引和减少数据拷贝**共同实现。

## 四、生产者：一条消息如何进入 Kafka？

Producer 的工作不是简单地调用一次网络写入。它需要完成分区选择、批量组织、请求发送、重试和确认处理。

### 4.1 Producer 的内部处理流程

```text
业务调用 send(record)
          │
          ▼
     序列化消息
          │
          ▼
     选择目标 Partition
          │
          ▼
     放入对应批次 RecordAccumulator
          │
          ▼
     Sender 线程组织 ProduceRequest
          │
          ▼
     发送给 Partition Leader
          │
          ▼
     Broker 追加日志
          │
          ▼
     根据 acks / ISR 等机制确认
          │
          ▼
     完成回调或返回发送结果
```

生产者通常通过 `RecordAccumulator` 缓存待发送记录，按分区组织批次，由后台 `Sender` 线程发送请求。网络连接、元数据刷新、重试和请求超时等机制共同完成发送过程。

### 4.2 为什么要按 Partition 批量发送？

假设每条消息大小为 1 KB，每秒产生 10 万条消息。如果每条消息单独发起网络请求，会造成大量请求头、系统调用、网络往返和 Broker 请求处理开销。

Kafka 可以将同一个 Partition 的多条消息合并成批次：

```text
Partition 0:
  Record 1 ┐
  Record 2 ├── RecordBatch A ──┐
  Record 3 ┘                   │
                              ├── ProduceRequest
Partition 1:                  │
  Record 4 ┐                  │
  Record 5 ┴── RecordBatch B ──┘
```

主要参数：

| 参数 | 作用 |
|---|---|
| `batch.size` | 控制批次缓冲的目标大小 |
| `linger.ms` | 允许生产者等待一段时间，积累更多记录 |
| `buffer.memory` | 控制生产者用于缓冲待发送记录的内存预算 |
| `compression.type` | 控制批次压缩算法 |
| `max.in.flight.requests.per.connection` | 控制单连接上允许的未完成请求数 |

`batch.size` 不是整个请求的最大大小，`linger.ms` 也不是消息发送必然等待的固定时间。批次可能因为容量已满等原因提前发送。

**性能上的本质：用更少的请求和更大的连续数据块，摊薄每条消息的固定开销。**

### 4.3 消息如何选择 Partition？

常见的分区策略有三类：

1. 显式指定 Partition：由业务直接决定写入哪个分区。
2. 指定 Key：通常根据 Key 计算分区，使相同 Key 的记录进入同一分区。
3. 不指定 Key：使用生产者的默认分区策略；具体行为取决于客户端版本和配置。

例如：

```text
key = user_id

hash(user_id) → Partition
```

如果 `user_id=1001` 的消息始终采用一致的分区映射，那么这个用户的消息就可以保持分区内的写入顺序。

但这也带来一个重要问题：**热点 Key**。如果某个 Key 对应的流量特别大，即使 Topic 有 100 个 Partition，也可能只有一个 Partition 承担绝大多数流量。增加 Partition 数量并不一定解决已有 Key 的热点问题，因为分区映射可能变化，而且同一个 Key 的顺序性通常要求它仍然进入同一个分区。

### 4.4 `acks` 决定什么？

`acks` 决定生产者需要等待怎样的 Broker 确认。

| 配置 | 语义 | 主要风险 |
|---|---|---|
| `acks=0` | 不等待 Broker 确认 | 生产者无法确认 Broker 是否成功写入 |
| `acks=1` | 等待 Leader 本地日志追加确认 | Leader 故障时，尚未复制的记录可能丢失 |
| `acks=all` | 等待当前 ISR 中所需副本完成确认 | 延迟更高；ISR 不足时可能拒绝写入 |

`acks=all` 不代表等待集群内的所有副本，也不等同于每条消息都立即执行磁盘 `fsync`。

例如，副本因子为 3，当前 ISR 有 3 个副本，且 `min.insync.replicas=2`：

```text
Leader:   Broker A
Follower: Broker B
Follower: Broker C

Producer --ProduceRequest--> A
                               ├── 本地追加
                               ├── B 复制日志
                               └── C 复制日志
```

如果使用 `acks=all`，确认需要考虑当前 ISR 内副本的复制情况；如果 ISR 缩小到 2 个，仍可能满足最小 ISR 要求。如果缩小到 1 个，则在 `min.insync.replicas=2` 的约束下，`acks=all` 写请求会失败。

### 4.5 重试为什么可能产生重复消息？

```text
Producer                  Broker
   │                         │
   ├──── 发送消息 M ─────────>│
   │                         ├── 已成功追加 M
   │<──── 响应在网络中丢失 ────X
   │
   ├──── 重试发送 M ─────────>│
   │                         ├── 可能再次追加 M
```

生产者收到超时，并不意味着 Broker 没有写入消息。因此，重试可能产生重复记录。

Kafka 通过幂等生产者机制降低这种重复风险：生产者使用 Producer ID（PID）、序列号等信息，让 Broker 能够识别生产者在同一分区上的重复批次。

启用幂等生产者后，Kafka 可以在相应的生产者会话和协议约束范围内避免重试造成的重复追加。但幂等性不是跨任意生产者会话、任意外部系统的全局去重；消费者业务仍可能因 Offset 提交失败而重复执行。端到端一致性还需要额外的事务、幂等或去重设计。

## 五、Broker 如何处理请求？

从逻辑上，可以理解为：

```text
TCP 连接
   ↓
网络接收与请求解析
   ↓
请求分发与处理
   ↓
校验 Topic / Partition / 权限等
   ↓
定位分区 Leader
   ↓
追加日志、更新相关状态
   ↓
等待所需的副本确认条件
   ↓
返回 ProduceResponse
```

Kafka 的具体网络实现和线程模型随版本演进，但需要区分三个方面：

- **网络层**负责接收连接和请求、发送响应。
- **请求处理层**负责执行协议请求对应的操作。
- **日志与副本层**负责记录数据、复制日志和判断确认条件。

这些环节并不意味着每条消息都要先经过一次独立的数据库式事务，也不意味着每条消息都必须同步刷盘。

一个 ProduceRequest 可以包含多个分区批次；一个批次可以包含多条记录；日志追加可以合并处理；副本也可以批量拉取 Leader 的日志。系统处理的基本单位经常是**批次和请求**，而非单条业务消息。

## 六、副本复制：Kafka 如何保证数据可靠性？

### 6.1 Leader 与 Follower

Kafka 的复制单位是 Partition。例如：

```text
Topic: orders
Partition: 0
Replication Factor: 3

Broker A：Leader
Broker B：Follower
Broker C：Follower
```

同一个 Partition 的多个副本维护相同逻辑顺序的日志，但它们的日志末尾可能暂时不一致。Leader 负责该分区的主要写入协调工作；**Follower 通过向 Leader 拉取数据来复制日志**。

Follower 主动拉取的优势包括：

- Follower 可以按批次拉取，降低请求开销。
- Follower 可以根据自身复制进度决定拉取位置。
- Leader 不必为每条消息单独推送一次数据。
- 复制机制可以复用日志批次和网络传输的优化路径。

### 6.2 ISR 是什么？

ISR（In-Sync Replicas）是当前被认为与 Leader 保持同步的副本集合。

```text
全部副本：{A, B, C}
当前 ISR：{A, B, C}

如果 C 长时间无法及时追上 Leader：
全部副本：{A, B, C}
当前 ISR：{A, B}
```

需要区分：

- 副本存在，不代表它一定属于 ISR。
- Broker 在线，不代表它的所有副本都一定同步。
- ISR 中的副本也可能存在尚未完成的磁盘持久化工作。

Kafka 会综合副本存活状态和复制进度判断同步状态。具体判定和控制参数会随版本变化，不能简单地将 ISR 等同于“所有消息都已写入物理磁盘的副本”。

### 6.3 LEO、HW 与高水位

- **LEO（Log End Offset）**：当前日志末尾的下一个 Offset，反映副本本地日志追加到了哪里。
- **HW（High Watermark）**：高水位边界，决定普通消费者能够读取到的已提交日志范围。

假设 Leader 的日志状态如下：

```text
Offset:  0  1  2  3  4  5  6  7
HW = 4
LEO = 8
```

`HW=4` 表示已提交日志的下一个 Offset 边界是 4，因此普通 `read_uncommitted` 消费者通常只能读取 Offset 小于 4 的记录。`LEO=8` 表示 Leader 本地日志末尾的下一个 Offset 是 8。中间的 4～7 已经被 Leader 追加，但还不一定全部满足复制提交条件。

Offset 4 只是 HW 边界，并不意味着一定存在一条 Offset 为 4 的记录。

### 6.4 消息什么时候算提交？

从复制语义上讲，Kafka 的提交与副本复制进度密切相关。在常见的 Leader/Follower 复制模型中，Leader 维护 ISR，并根据副本日志进度推进高水位。普通消费者只能读取已提交范围内的记录，而生产者确认还取决于 `acks` 的配置。

需要区分两个概念：

- **生产者确认（Producer acknowledgment）**：生产者何时收到成功响应。
- **消费者可见性（Consumer visibility）**：消息何时进入允许消费者读取的范围。

二者密切相关，但不是完全相同的概念。尤其是事务消息，`read_committed` 消费者还会受 Last Stable Offset 等事务边界约束。

### 6.5 Leader 故障后如何恢复？

```text
正常运行：
A = Leader
B = Follower
C = Follower

A 突然宕机：
Controller 发现 A 不可用
          ↓
评估可用副本与当前 ISR
          ↓
选举新的 Leader，例如 B
          ↓
更新分区元数据
          ↓
Producer / Consumer 刷新元数据
          ↓
继续读写
```

关键不在于“随便找一个在线副本当 Leader”，而在于**尽量从符合选举条件且数据足够新的副本中选出新 Leader**。

如果允许不在 ISR 中的落后副本成为 Leader（即不干净的 Leader 选举），可能牺牲数据一致性以换取可用性。因此，生产环境通常需要慎重配置 `unclean.leader.election.enable`。

### 6.6 Kafka 是不是多数派共识？

不能简单地把 Kafka 的 ISR 复制模型等同于 Raft 或 Paxos。Kafka 的数据复制与提交使用 Leader、Follower、ISR、高水位等机制；现代 Kafka 的 KRaft 元数据控制平面则使用基于 Raft 的共识机制。

两者解决的问题不同：

- **数据平面**：Partition 日志如何复制、提交与故障恢复。
- **控制平面**：集群元数据如何达成一致、Controller 如何选举与切换。

KRaft 并不是把每个业务 Partition 的所有数据都改成由 Raft 多数派逐条复制的简单模型。

## 七、消费者：Kafka 如何实现消费进度与并行处理？

Kafka Consumer 的设计重点是：**消费者自己控制读取位置，Broker 负责提供日志数据，Consumer Group 负责协调分区的消费归属。**

### 7.1 Consumer 拉取消息的流程

```text
Consumer 启动
    ↓
获取集群元数据
    ↓
加入 Consumer Group
    ↓
获得 Partition 分配结果
    ↓
向相应 Broker 发送 FetchRequest
    ↓
指定 Partition 与读取 Offset
    ↓
Broker 返回一批消息
    ↓
Consumer 执行业务处理
    ↓
提交消费 Offset
```

消费者通过 Fetch 请求指定从哪个 Offset 开始读取，并根据配置控制每次获取的数据量。Kafka 不要求每消费一条消息，就向 Broker 发送一次独立的确认请求。消费者可以批量获取、批量处理，并按一定策略提交进度。

### 7.2 Consumer Group 如何实现并行消费？

假设 Topic 有 6 个 Partition：

```text
Topic：orders
P0 · P1 · P2 · P3 · P4 · P5

Consumer Group：order-service
C1 → P0、P1
C2 → P2、P3
C3 → P4、P5
```

在一个 Consumer Group 内，一个 Partition 在同一时刻通常只分配给一个组成员消费。因此：

- 6 个 Partition、3 个 Consumer：可以分配给每个消费者 2 个分区。
- 6 个 Partition、6 个 Consumer：每个消费者可以获得 1 个分区。
- 6 个 Partition、8 个 Consumer：至少有 2 个消费者无法获得分区。

这意味着**一个 Consumer Group 的有效消费并行度受到可分配 Partition 数量的限制**。

如果业务希望同一份消息同时被订单服务和数据分析服务独立消费，应使用不同的 Consumer Group，而不是将两个业务放进同一个 Group。

### 7.3 Offset 提交：为什么会重复消费？

假设 Consumer 读取到 Offset 100，并完成了业务处理：

```text
读取消息 100
    ↓
业务处理成功
    ↓
提交 Offset
```

如果业务处理成功，但在提交 Offset 之前 Consumer 宕机，那么恢复后可能再次读取 Offset 100 对应的消息。

反过来，如果先提交 Offset，再执行业务处理，而 Consumer 在业务处理之前崩溃，恢复后就可能跳过尚未完成的业务。

因此：

- 先处理、后提交：通常偏向至少一次处理，可能重复。
- 先提交、后处理：可能产生业务丢失或未处理记录被跳过的风险。
- 将 Offset 与业务状态放在同一事务边界中：可以在适用条件下解决更严格的一致性问题。

实际系统中，外部数据库业务经常采用幂等写入、唯一业务键、去重表或数据库事务等手段处理重复执行。

### 7.4 Consumer Group 如何进行 Rebalance？

当组成员加入、退出、崩溃或订阅发生变化时，Kafka 可能重新分配 Partition，这个过程称为 Rebalance。

```text
初始：
C1 → P0、P1、P2
C2 → P3、P4、P5

C3 加入后：
C1 → P0、P1
C2 → P2、P3
C3 → P4、P5
```

Rebalance 的好处是可以自动调整并行度和故障恢复，但也有代价：

- 重新分配期间可能暂时无法正常消费。
- 可能增加延迟和重复处理。
- 频繁成员变化会造成持续的协调开销。

Kafka 提供不同的分区分配策略和再均衡机制。经典的 Eager Rebalance 通常需要先撤销旧分配；Cooperative Rebalance 则允许在一定条件下逐步迁移分区，减少不必要的停顿。

### 7.5 Kafka 的消费进度存储在哪里？

对于使用 Kafka Consumer Group 的消费者，已提交 Offset 通常由 Kafka 内部的 `__consumer_offsets` Topic 管理。

```text
Consumer Group: order-service

Topic orders:
  Partition 0 → committed offset 100
  Partition 1 → committed offset 235
  Partition 2 → committed offset 418
```

这不是对每条消息单独维护一份消费状态，而是维护每个消费组、分区对应的进度记录。消费者在故障恢复或分区重新分配后，可以从已提交的位置恢复。

但要注意：**已提交 Offset 代表消费者保存的进度，不代表对应业务一定已经成功完成。** 这取决于应用如何安排业务处理和 Offset 提交。

## 八、日志保留、删除与压缩

Kafka 采用日志模型，不意味着消息会无限期保留。它提供两类非常重要的清理方式。

### 8.1 按时间或容量删除

例如：

```text
Topic retention.ms = 7 天

Segment A：超过保留期限
Segment B：仍在保留期限内
Segment C：当前活跃 Segment
```

后台清理机制可以删除已经满足条件的旧 Segment，而不是逐条执行消息删除。常见参数包括：

- `retention.ms`：按时间保留。
- `retention.bytes`：按日志容量控制保留范围。
- `segment.bytes`：控制日志段的目标大小。

具体删除时机还受到 Segment 滚动、检查周期和其他配置影响。达到保留条件不意味着某条消息会立刻被删除。

### 8.2 Log Compaction：按 Key 保留最新状态

另一种方式是日志压缩（Log Compaction）。例如，某个 Topic 保存用户状态：

```text
Offset 0: key=user1001, value=普通用户
Offset 5: key=user1001, value=VIP 用户
Offset 9: key=user1001, value=超级 VIP
```

压缩后，旧状态可能被清理，最新状态得以保留：

```text
key=user1001 → 超级 VIP
```

与按时间删除不同，压缩的主要目标不是保留最近几天的全部事件，而是保留每个 Key 的最新状态。适合的场景包括：

- 用户或设备最新状态。
- 配置变更日志。
- 需要重建状态的变更流。
- 以 Key 为主的状态同步。

压缩不是立即生效的逐条覆盖操作，而是后台清理和重写日志段。它也不保证消费者在任何时刻都只看到每个 Key 的一条记录。

## 九、事务与 Exactly-Once：Kafka 能保证什么？

### 9.1 三种不同的语义

| 语义 | 含义 |
|---|---|
| At-most-once | 消息最多处理一次，但可能遗漏 |
| At-least-once | 消息至少处理一次，但可能重复 |
| Exactly-once | 在规定的事务边界内，结果只产生一次有效效果 |

**Kafka 的幂等生产者解决的是生产端重试引起的重复追加问题**。**Kafka 的事务机制则可以进一步协调多个 Kafka 分区的写入，并将消费 Offset 纳入同一事务。**

例如，实现一个消费 Topic A、处理后写入 Topic B 的数据处理任务：

```text
读取 Topic A
     ↓
执行转换
     ↓
开启 Kafka 事务
     ├── 写入 Topic B
     └── 将 Topic A 的消费 Offset 纳入事务
     ↓
提交事务
```

如果事务成功提交，输出记录和消费进度可以在 Kafka 的事务语义下保持一致；如果事务中止，`read_committed` 消费者不会将中止事务中的数据作为已提交结果读取。

### 9.2 Exactly-Once 的边界

Kafka 的 Exactly-Once 并不自动意味着整个业务系统实现了端到端 Exactly-Once。假设消费者还要调用外部支付系统：

```text
读取 Kafka 消息
    ↓
调用支付系统成功
    ↓
Kafka 事务提交失败
    ↓
消息重新处理
```

如果支付系统没有幂等机制，支付操作仍然可能重复。

因此需要明确区分：

- Kafka 内部生产、消费与事务的语义。
- 外部数据库、HTTP 服务、支付系统等副作用的语义。
- 整个业务流程的端到端一致性。

跨越 Kafka 与外部系统时，通常仍需幂等键、事务性 Outbox、Inbox 去重或业务补偿等方案。

## 十、KRaft：Kafka 如何管理集群元数据？

Kafka 早期依赖 ZooKeeper 管理集群元数据和 Controller 选举。现代 Kafka 使用 KRaft，将元数据管理集成进 Kafka 自身的控制平面。

可以把集群分成两部分：

- **数据平面（Data Plane）：** 负责 Topic Partition 的日志写入、读取、复制和消费。
- **控制平面（Control Plane）：** 负责 Broker 注册、集群元数据、分区 Leader 状态、Controller 选举和元数据变更。

KRaft 的核心是使用一组 Controller 形成元数据仲裁组，通过 Raft 共识机制维护元数据日志。

```text
Controller 仲裁组
       ↓
选举 Active Controller
       ↓
接收元数据变更
       ↓
复制元数据日志
       ↓
达成提交条件
       ↓
向 Broker 发布元数据更新
```

需要注意：

1. Active Controller 负责协调集群元数据变更。
2. 其他 Controller 参与元数据日志的复制与共识。
3. Broker 根据集群元数据处理实际的分区读写。
4. 数据 Partition 的 Leader/Follower 复制机制与 KRaft 元数据日志的 Raft 复制是不同层面的机制。

KRaft 的价值不仅是减少对 ZooKeeper 的外部依赖，还在于把元数据管理和集群控制流程统一到 Kafka 自身的架构中。

学习源码时，可以分别沿着两个方向理解：一条是 Partition 数据复制与日志提交，另一条是 KRaft 元数据日志与 Controller 状态机。

## 十一、Kafka 高吞吐的根本原因

前面介绍了各个组件，现在把它们放在一起，就能理解 Kafka 的性能优势从哪里来。

1. **批量化：** Producer、Broker、Follower、Consumer 都可以按批次处理消息，减少每条消息承担的固定开销。
2. **顺序日志与页缓存：** 以追加写为主，避免大量随机更新；借助操作系统页缓存提升读写效率。
3. **高效网络传输：** 通过批量网络请求、减少数据拷贝，以及适用场景下的零拷贝机制降低传输开销。
4. **Partition 并行化：** 不同 Partition 可以分布在不同 Broker 上并行处理，消费也可以按分区并行扩展。
5. **统一日志格式与压缩：** 生产、存储、复制和消费尽量复用批次数据结构；压缩可以减少网络和磁盘带宽消耗。

这些机制共同减少了单位消息消耗的 CPU、网络、磁盘 I/O 和内存带宽。不过，批次过大可能增加等待延迟和内存占用；压缩可能增加 CPU 开销；副本确认也会增加写入延迟。因此，吞吐量、延迟、可靠性需要结合实际业务进行权衡。

## 十二、生产环境如何调优？

不要一开始就盲目增加 Broker 数量。应先确定瓶颈发生在哪一层。

| 观察到的问题 | 优先检查 |
|---|---|
| Producer 吞吐低 | `batch.size`、`linger.ms`、压缩、分区热点、请求延迟 |
| Broker 磁盘繁忙 | 日志写入量、页缓存、磁盘延迟、Segment 清理与压缩 |
| Broker 网络繁忙 | 副本流量、消费者流量、请求大小、网卡带宽 |
| Consumer 积压 | 实际处理耗时、消费并行度、分区数、外部依赖延迟 |
| Rebalance 频繁 | 消费者稳定性、处理超时、心跳、分配策略 |
| 副本持续落后 | Leader 写入压力、网络带宽、Follower 磁盘能力 |
| 写入偶发失败 | ISR 数量、`min.insync.replicas`、Leader 变更、超时与重试 |
| 延迟突然升高 | GC、CPU 饱和、磁盘延迟、网络拥塞、批次等待 |

### 12.1 可靠性优先的配置示例

```properties
# Producer
acks=all
enable.idempotence=true

# Topic / Broker
replication.factor=3
min.insync.replicas=2
```

这里的副本因子和最小 ISR 是示例配置，实际还应考虑故障域分布、可用性目标和 Kafka 版本。`min.insync.replicas=2` 本身不会自动保证生产者使用 `acks=all`，两者需要配合。

### 12.2 吞吐优先的配置示例

```properties
batch.size=65536
linger.ms=5
compression.type=lz4
```

这些值只是调优起点，不是通用最优值。应通过实际消息大小、流量模式和延迟目标进行压测。

### 12.3 消费扩展

增加 Consumer 数量之前，先确认 Partition 数量是否足够，并检查业务处理是否能安全并行。如果瓶颈在数据库写入或第三方 API，单纯增加 Consumer 反而可能加重下游压力。

## 十三、理解 Kafka 源码的建议顺序

如果目标是掌握设计与实现，而不仅仅是使用 Kafka，我建议按以下顺序阅读源码。

1. **消息与日志存储：** 从 Record、RecordBatch、`UnifiedLog`、`LogSegment`、Offset 索引入手，理解消息如何编码、追加、定位和清理。
2. **生产者实现：** 阅读 `KafkaProducer`、`RecordAccumulator`、`Sender`，理解批次组织、网络发送、重试和幂等性。
3. **Broker 请求处理：** 研究 Produce、Fetch 请求处理流程，理解请求如何进入日志层，以及确认响应何时返回。
4. **副本复制：** 研究 `ReplicaManager`、副本日志进度、ISR、HW 和 Leader 变更。这是理解 Kafka 高可用的关键。
5. **消费者与组协调：** 阅读 `KafkaConsumer`、`ConsumerCoordinator`、分区分配和 Offset 提交流程，重点理解 Rebalance 与重复消费。
6. **KRaft 与事务：** 最后学习 Controller、元数据日志、Raft 状态机，以及事务协调器和事务提交过程。

### 官方资料

- Apache Kafka 官方设计文档：[Design and Implementation](https://kafka.apache.org/41/design/design/)
- 官方协议文档：[Kafka Protocol](https://kafka.apache.org/39/design/protocol/)
- 官方源码：[apache/kafka on GitHub](https://github.com/apache/kafka)

## 最后总结：Kafka 的核心设计逻辑

理解 Kafka，可以归结为五个问题：

1. **如何高效存储？** 追加日志、Segment、稀疏索引、页缓存。
2. **如何高效传输？** 批量请求、批次压缩、减少数据拷贝。
3. **如何扩展吞吐？** Partition 分片、Broker 分布、Consumer Group 并行消费。
4. **如何应对故障？** Leader/Follower、ISR、高水位、Leader 选举。
5. **如何保证业务正确性？** 生产者确认、幂等生产、Offset 提交、事务和外部系统幂等。

**最值得建立的整体认识是：Kafka 首先是一套分布式、可复制的追加日志系统，其上才构建了消息队列、消费组和流处理能力。** 正是这个底层模型，让高吞吐、消息回放、多消费者独立读取和持久化保留能够统一在同一套架构中。
