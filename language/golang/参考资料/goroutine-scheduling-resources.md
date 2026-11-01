# Golang Goroutine 调度 - 书籍与文章推荐

---

## 📚 书籍

| 优先级 | 书名 | 调度相关深度 |
|--------|------|-------------|
| 🥇 | **《Go语言设计与实现》**（左书祺/Draven） | 中文社区公认最深。源码级剖析调度器，包含 GMP 每个组件的精确数据结构定义、`schedule()` 循环的完整流程、四种调度触发时机、工作窃取与 Hand Off 机制的实现细节 |
| 🥈 | **《深入理解Go语言》**（刘丹冰/Aceld） | 图文并茂讲解 GPM 模型，适合建立调度机制的直觉理解 |
| 🥉 | **《Go底层原理与工程化实践》** | 前半部讲调度、GC 等原理，后半部落到工程实践 |

## 🌐 博客 / 在线资源

| 资源 | 亮点 |
|------|------|
| **draveness.me/golang/** | 《Go语言设计与实现》的在线版，调度器章节免费，源码级讲解 |
| **《Go 语言原本》** | 开源书，从源码工程角度剖析调度器演进 |
| **幼麟实验室**（公众号/B站） | 视频形式讲解 GMP 模型，可视化程度很高 |

---

## 📄 重点文章链接

### GMP 调度器深度解析

1. **[Go调度器GMP模型深度拆解（Go 1.22源码 + GDB实时调度轨迹还原）](https://datasea.cn/go0414592863.html)**
   — 揭示 P 窃取失败的真实频率，结合 GDB 实战

2. **[协程调度器源码级解读：findrunnable() 函数的5个关键分支](https://datasea.cn/go0401571269.html)**
   — 基于 Go 1.22 `src/runtime/proc.go`，逐分支分析调度决策逻辑

3. **[Go调度器源码解读：goroutine调度核心逻辑解析](https://www.jjblogs.com/threads/182531)**
   — 调度循环 `schedule()` 函数完整走读

4. **[深入分析 Go1.18 GMP 调度器底层原理](https://zhuanlan.zhihu.com/p/586236582)**
   — 知乎上广受好评的 GMP 深度文章

5. **[Golang - 协程 Work Stealing 机制（GMP模型）](https://www.cnblogs.com/beatle-go/p/17895147.html)**
   — 聚焦工作窃取机制的实现细节

### 抢占式调度

6. **[Go 1.14 非协作抢占式调度原理](https://developer.aliyun.com/ask/375373)**
   — 信号式异步抢占的实现机制

7. **[从源码剖析 Go 语言基于信号抢占式调度](https://cloud.tencent.com.cn/developer/article/1808073?policyId=1004)**
   — 信号抢占的完整源码分析

8. **[Go 语言的信号式抢占原理](https://blog.csdn.net/cch1234/article/details/119113759)**
   — SIGURG 信号的发送与处理流程

### 官方源码参考

9. **[runtime/HACKING.md](https://golang-org.appspot.com/go.dev/src/runtime/HACKING)**
   — Go 官方 runtime 源码导读，调度器部分必读

10. **[src/pkg/runtime/proc.c（早期调度器源码）](https://raw.githubusercontent.com/golang/go/1e112cd59f560129f4dca5e9af7c3cbc445850b6/src/pkg/runtime/proc.c)**
    — 调度器的历史版本，帮助理解 GMP 模型为什么是现在的样子

### 综合 / 面试

11. **[高端 Go 百万并发 — 深入协程调度：GMP 模型原理与高并发工程化实践](https://cloud.tencent.cn/developer/article/2698197?policyId=1004)**

12. **[从源码分析 GMP 调度原理](https://www.cnblogs.com/daemon365/p/18593573)**

13. **[Go 语言 GMP 调度模型深度解析](https://www.cnblogs.com/yangykaifa/p/19499349)**

14. **[深入分析 Go1.18 GMP 调度器底层原理（腾讯云版）](https://cloud.tencent.cn/developer/article/2176651)**

---

> 如果时间有限，优先读 **draveness.me 的调度器章节** + 前 4 篇文章，覆盖面最全。
