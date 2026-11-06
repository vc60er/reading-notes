# Claude 5 代模型的上下文工程新规

> 来源：官方博客《The new rules of context engineering for Claude 5 generation models》，Thariq Shihipar，2026-07-24（已抓原文）。
> 定位：这篇是 **对 01 笔记（2025 best practices）的部分修订**。针对 Claude Opus 5 / Fable 5 这类新模型，Anthropic 删掉了 Claude Code 系统提示词的 **80%+** 而编码评估**零损失**。

---

## 核心洞察

- Anthropic 之前**过度约束**了 Claude Code——系统提示词、CLAUDE.md、skills 里塞满互相冲突的指令（同一请求里「leave documentation as appropriate」和「DO NOT add comments」打架）。
- 旧模型需要这些 guardrail 兜底最坏情况；**新模型判断力更强，可以删掉很多，让模型用周围上下文 + 自己的判断**。
- 配套命令：`claude doctor`（Claude Code 里 `/doctor`）能自动帮你「rightsize」skills 和 CLAUDE.md。

## 六条「Then → Now」对照（核心）

| 旧（Then） | 新（Now） | 关键变化 |
|---|---|---|
| 给 Claude 规则 | 让 Claude 判断 | 旧：「默认不写注释、禁止多行 docstring」。新：「**写的代码和周边一致——匹配它的注释密度、命名、惯用法**」。 |
| 给 Claude 例子 | 设计接口 | 例子会把人框进特定探索空间；不如把**工具/脚本/文件的参数设计得有表现力**（如 Todo 工具用 `pending/in_progress/completed` 枚举暗示用法）。 |
| 一次性全塞进去 | 渐进披露（progressive disclosure） | 把 code review / 验证移进独立 skill 按需调用；工具用「延迟加载」（用 ToolSearch 按需拉全定义）；CLAUDE.md 用文件树按需加载。 |
| 重复自己 | 简洁的工具描述 | 工具用法写进 **tool description**，别在系统提示词里重复。 |
| 记忆放 CLAUDE.md | 自动记忆 | 旧的 `#` 快捷键写 CLAUDE.md **已被取代**——Claude 现在**自动保存**相关记忆。 |
| 简单 spec | 丰富引用 | spec/plan 可以是 HTML artifact、代码（测试套件 / 某个函数）、**rubric**（用动态工作流派 verifier agent 验证「品味」）。 |

## 落地到你的上下文

- **System prompt**：和产品强绑定，Claude Code 用户基本**不该改**；自己搭 agent harness 才需要花大功夫在这里。
- **CLAUDE.md**：保持**轻量**，简要说明 repo 是干嘛的；**大部分 token 花在「gotchas」（代码库里的坑/怪癖）**上，别写「看文件系统就知道的显然事」。重度用渐进披露（验证流程做成 skill，从 CLAUDE.md 引用）。
- **Skills**：当「轻量指南」，让 Claude 按需找信息，**别过度约束**（除非是极重要的领域）；长 skill 拆多文件；最适合放「你 / 团队 / 产品的特定观点、知识、最佳实践」。
- **References**：用 `@` 引用文件当参考；**优先用代码当参考**——HTML mockup 通常比文字描述或截图效果更好。

---

## ⚠️ 与 01 笔记的冲突点（务必看）

1. 01 §4.1 的「`#` 键写 CLAUDE.md 记忆」→ **已被自动记忆取代**（本条「Then：记忆放 CLAUDE.md」）。
2. 01 反复强调「具体、可验证、加 YOU MUST」的大方向仍对，但针对 Claude 5 代要**克制**：别把 CLAUDE.md 写成「所有已知实践的中央仓库」，要写成「**gotchas + 渐进披露的索引**」。
3. 01 的「给 Claude 明确规则兜底」思路，在 Claude 5 代上逐步让位于「让 Claude 判断」——**约束越少、越靠工具/接口设计表达意图**。
