# Claude Code 创业公司五条运营原则

> 来源：官方博客《The Claude Code guide for startups》，Michael Segner，2026-08-20（已抓原文）。访谈 12+ 家高速成长创业公司（ClickHouse、Clay、Artemis Security、Commure、Crosby、Heidi、Omni、Translucent 等）。
> 定位：团队/组织级的 vibe coding 落地方式，是对 01 个人最佳实践的补充。

---

## 01 Everyone ships —— 人人都能发布

门槛降低后，「最懂问题的人」能直接 ship 第一版，从 0→1 对所有人开放（律师、市场都能发 PR），但仍有分工。

- **Claude 看不到的，它就没法用**：用 MCP 连真相源和团队日常工具；已有成熟 CLI 的工具（`gh`/`kubectl`/`bq`/`psql`）直接连，更省 token。
- **skill 目录跨团队共享**：一个人的最佳实践立刻变成另一个人的。
- **子目录 CLAUDE.md** 放该目录的编码约定（每次生效）；**skill** 放按需触发的流程。

## 02 Automate the tedium —— 把琐事自动化

让 agent 接管机械的 80%，工程师专注需要判断的 20%。

- **Code Review（research preview）**：托管的多 agent PR 自动审查，按严重度打标，`@Claude` 评论闭环（需配 GitHub Actions）。
- **Claude Tag**：CI/CD 事故 on-call 第一响应者（Anthropic 自己用它，15 分钟内出第一份情况报告）；指令放 markdown skill、进 git 管理。
- 案例：ClickHouse 的「修 flaky test」「找缺失测试覆盖」两个 agent 已是 repo 的 **#2/#3 贡献者**。
- **动态工作流 fan-out**：对 Opus/Fable 说「fan out multiple subagents」或「use a workflow」，并行分析海量数据或做对抗式审查。

## 03 Trust, but verify —— 信任，但要验证

不能验证就没法自动化。Artemis 的飞轮：**结构好代码库 / 知识库 / 团队，每次贡献都复利**。

- **把不能变的东西写进根目录 CLAUDE.md**：架构规则、安全边界、non-negotiables（Zingage 写了 567 行「团队怎么想」）。
- 高度监管场景（医疗编码 Cainex）：**hooks 当硬闸门**（每次必执行，不管模型怎么想）；「**fix the principle, not the example**」（修原则，不修个例）；维护 **golden set evals + 回测** 防漂移。
- **loops** = agent 循环到 stop condition（flaky test agent：自己重跑测试到绿，停条件清晰自洽）。
- 反例教训：第一版「overfit」会把具体 case 编码进去、越改越堆积补丁——要强制通用原则、限制单次改动里的具体细节数量。

## 04 Build for rebuilding —— 为重建而建

模型能力一直在变，没什么是永久的。「build it four times」。

- **git worktree 跑重建**：v2 与 v1 并排、各占一个 branch，跑 evals 对比，赢了才合——这让「建四遍」变得便宜。
- 重建的终点不是「新路径上线」，而是「**旧路径删除**」（Commure 用一个 skill 批量生成「删除已全量放开的 feature flag 及关联代码」的 PR）。
- 非平凡重写用 **plan mode**（`--plan` / Shift+Tab）——最便宜地发现架构漂移。

## 05 Prototype → dogfood → productionize —— 原型 → 吃狗粮 → 产品化

- 内部用 Claude Code 建 agent → 自己先用（dogfood）→ 视反馈升级成客户产品（Claude API / SDK / Managed Agents）。
- 用 AI 建 AI 产品本身也反哺对模型能力和 harness 设计的理解（Omni 借鉴了 Anthropic 的「file vs embedding」选择，省掉了 RAG 的复杂度）。

---

## 一页清单（官方整理）

1. **连接真相源**：MCP / CLI 连团队日常工具与数据源。
2. **共享**：公司级 plugin / skill marketplace；子目录 CLAUDE.md 放约定；skill 放流程。
3. **自动审查**：仓库开 Code Review；Claude Tag 进 CI/CD on-call 与 bug triage。
4. **并行/对抗**：动态工作流 fan-out 子代理分析或对抗式审查。
5. **不可变项**：根目录 CLAUDE.md 放架构规则与安全边界。
6. **长期任务**：用 loops（循环到停条件）。
7. **防漂移**：建立并维护 agent evals（golden set）。
8. **确定性闸门**：hooks 当硬闸门（阻断不合规写入、提交前跑测试）。
9. **重建**：git worktree 隔离重建；重写用 plan mode。
