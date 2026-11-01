# 验证闭环（Verification Loops）：把人工检查做成 skill

> 来源：官方博客《Building verification loops in Claude Code with skills》，Delba de Oliveira（Claude Code 团队），2026-07-22（已抓原文）。
> 定位：对 01 §5「测试与验证」、03「loops」的落地补充——**把「你每次都手动复查的那些步骤」固化成 skill，让 Claude 自己闭环检查→修复**。

---

## 一句话

Claude 已经能靠确定信号（类型检查、linter、测试、运行时错误）自我验证一部分；**凡是你每次都要手动复查的步骤，都能做成「验证闭环」**——Claude 检查 → 失败 → 修复 → 再检查，直到通过，你可以去做别的。

## 内置的验证闭环（先用现成的）

- **`/verify` skill**：构建 + 运行 + 观察改动。
- **工具链**：Claude 会捕获你提供的任何工具（linter 等）的错误码/警告。**把准确的构建/测试命令写进 CLAUDE.md**，别让 Claude 去猜。
- **Code Review（research preview）**：托管多 agent PR 自动审查，`@claude` 评论闭环。
- **GitHub Actions**：定义一个调用 Claude + 验证 skill 的 job，本地跑的检查在每次 push/PR 也自动跑。
- **Spec validation**：skill 核对每个改动是否符合仓库里的 markdown spec 并修违规。
- **Rubrics in Claude Managed Agents（beta）**：用独立的 grader agent 按 rubric 打分，失败自动回炉重做。

## 怎么写自己的验证闭环

1. 把「每次 Claude 实现完功能后，你都要做的那些小修正」**写下来**。
2. 写不出来就问 Claude 要该领域的最佳实践，再改——**你和它的差异点，正是你最该固化的地方**。
3. 检查不必是「定性」的：「拒绝任何**没有 backfill 步骤就删列的迁移**」这种确定性规则，通用 linter 抓不到、但项目特定 loop 能抓到。**凡是你一直靠手工强制的东西，都够格写成 loop**。

## 四种触发方式（把检查放到它该在的地方）

| 方式 | 何时触发 | 适用 | 代价/注意 |
|---|---|---|---|
| **Standalone（独立）** | 你手动调 | 跨切面、不必每次都跑的检查（pre-commit 安全扫描、pre-PR 可访问性审计、license 头检查） | 每次调用都要你记得；当「每次改完都要跑」时，就该升级了 |
| **Embedded（内嵌）** | 随产出 skill 自动跑 | 属于某个特定工作流的检查（在 scaffold 组件 skill 末尾加一行「跑 eslint」） | 只能改**你自己写的 / 项目级** skill；内置和插件托管的 skill 会随更新被覆盖，别用这招 |
| **Chained（链式）** | 一个 skill 结尾调另一个 | 多个已验证的交接端到端跑通 | 把「习惯」变「契约」；**增加 token 消耗，先测再铺开** |
| **On every PR（每次 PR）** | 每次 PR 自动跑 | 团队级：同事的改动过和你一样的门 | 链还在频繁变时先别上 PR 级，否则每次调整都是团队可见事件 |

**Anthropic 内部链式范例**：`/code-review`（找 bug）→ `/simplify`（清理 diff）→ `/verify`（确认端到端行为）→ 若动了 UI 再 `/design`（对照 DESIGN.md）。

## 最小 skill 示例

```markdown
# .claude/skills/verify-log-hygiene/SKILL.md
---
name: verify-log-hygiene
description: Check that error logs include the request ID and never include the
  request body. Use when the diff touches error handling or logging.
allowed-tools: [Read, Edit, Grep]
---
Read the error-handling paths in the current diff.
For each log call on an error path, confirm it includes the request ID and does
not pass the request body, headers, or any user-supplied payload.
Report each violation with file:line, then fix it.
```

## 落地流程（官方建议）

1. 挑这周你**做最多次**的那个手动复查步骤。
2. 先试内置 `/verify`。
3. 用大白话写流程（像交给第一天入职的新同事）。
4. 交给 `skill-creator` 插件，或自己丢个 markdown 进 `.claude/skills/`。
5. 在新任务上调它、确认检查真的跑起来，再迭代。
6. 用 skill 链串成端到端验证流。

> 你固化的越多，Claude 首答越接近你要的；省下来的注意力，去干那些**没有 skill 能替你写下来**的事。
