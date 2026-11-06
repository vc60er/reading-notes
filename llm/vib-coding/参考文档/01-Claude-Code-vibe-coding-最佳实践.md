# Claude Code Vibe Coding 最佳实践

> 范围：Claude Code 生态（agentic CLI）。覆盖方法论/工作流 + 工具链/具体技巧。
> 标注约定：🏛️ = 官方推荐（Anthropic）｜👥 = 社区共识（多个来源交叉验证）｜⚖️ = 存在取舍、需按场景判断。
> 整理时间：2026-08-21
> ⚠️ **模型代际提示**：本笔记主体是 2025 年口径。若你用 **Opus 5 / Fable 5** 等新模型，优先读 [02](./02-Claude-5-上下文工程新规.md)——部分「写具体规则、加 YOU MUST」的建议已被「让模型判断 + 用工具/接口设计表达意图」取代。

---

## 一句话结论

Vibe coding 的上限**不取决于 prompt 写多长，而取决于你预先立好的「工程约束」**。行业正在从「给 AI 更多工具」转向「给 AI 更多纪律」——代码变便宜之后，**工程判断力**反而更值钱。你必须能解释生成的代码为什么这么写、它怎么失败，再决定要不要合入。👥

---

## 1. 心智模型 🏛️

- **定位**：Claude Code 是 low-level、unopinionated 的工具，接近「裸模型访问」。它故意不强制某种工作流，所以最佳实践得你自己建。
- **把它当成「只用终端、从不用 GUI 的新同事」**：它不用代码索引，靠 **agentic search**（`grep`/`glob`/`find`），由模型自己决定怎么搜——跟新人探索代码库一个样。这解释了为什么要给它明确的 CLAUDE.md 和精确的任务边界。
- **信任边界**：AI 的输出在验证前一律视为不可信。密钥不进 prompt，写操作默认要你确认。

---

## 2. 核心工作流 🏛️👥

**标准循环：Explore → Plan → Code → Commit**

1. 让 Claude 读相关文件/URL，**但明确告诉它先别写代码**。
2. 让它出计划——用 `think` / `think hard` / `ultrathink` 触发扩展思考。
3. 复杂问题**尽早用 subagent**，保护主上下文。
4. 考虑让它把计划写成文档或 GitHub issue。

### Plan Mode（计划模式）——最重要的一道闸门 🏛️

- 系统级「意图闸门」：Claude **物理上无法写代码**，直到你批准计划。
- 切换：`Shift+Tab`（连按两次）或 `/mode`；也可 `claude --permission-mode plan` 启动。
- 计划可编辑（`Ctrl+G`），**实现前必须过人工审查**——「跳过规划阶段是致命的」👥。
- **非平凡任务默认开 plan mode**；小任务（改变量名、加日志）跳过，别过度仪式感 ⚖️。
- 计划模式会并行派发 **Explore 子代理**（轻量 Haiku）分头搜代码库，只回摘要给主代理。

### 拆解顺序（社区高度一致）👥

1. **先立「项目宪法」**：技术栈/依赖、目录与命名规范、安全与异常红线、统一返回结构。
2. **最小可运行骨架**：脚手架、路由、统一异常处理、健康检查、依赖清单——先跑通再写业务。
3. **按单一职责拆模块**，增量生成：config → 工具 → 核心逻辑 → 接口。
4. **大改动前先 git 快照/提交**，留回滚路径。

### Harness Engineering（工作流插件）👥

社区流行把固定流程固化成插件/skill，强制 Brainstorm → Plan → TDD → Review → Finish：
- [superpowers](https://github.com/obra/superpowers)、[tworkflow](https://github.com/clarity-digital-development/tworkflow)（40% 上下文规则）、[cavekit](https://github.com/JuliusBrussee/cavekit)、[Continuous-Claude](https://github.com/namesreallyblank/Continuous-Claude-v3)。

---

## 3. 上下文管理——最重要的资源 🏛️👥

核心原则：**上下文是头号资源**，窗口填满质量就下降，且 AI 无法自测这种下降。

- `/clear` 清空重开（无关任务换新会话）；`/compact` 压缩；`/save-session` 存档。
- **40% 规则**（tworkflow）：把上下文窗口当预算，在质量下降**之前**主动 reset，而不是之后。👥
- **探索交给 subagent**：它读几十个文件、只回摘要，主会话保持干净。🏛️
- **跨会话持久记忆**：`HANDOFF.md` / `SPEC.md` / 连续性 ledger，在 CLAUDE.md 里引用，让知识扛过上下文重置。👥
- `Esc` 打断并纠正；`Esc`+`Esc` 回到会话起点（配合 checkpoint）。
- `/context` 看占用、`/usage` 看限额。

---

## 4. 机制怎么选：官方「七法」——CLAUDE.md / Rules / Skills / Subagents / Hooks / Output styles / append system prompt 🏛️

官方《Steering Claude Code》（2026-06-18）把定制 Claude 行为的方法归为**七种**，每种只控制三件事：**何时加载进上下文、能否扛过 compaction（压缩）、权威性多高**。选型启发式：

| 需求 | 用什么 |
|---|---|
| 需要 Claude **知道**的项目事实 | CLAUDE.md |
| **必须发生**的事（确定性执行） | Hook |
| **可重复的流程** | Skill |
| 会**污染主上下文**的旁支任务 | Subagent |

**七法关键差异速记**（compaction 是重点）：CLAUDE.md 根文件「memoized，压缩后重读」；子目录 CLAUDE.md / path-scoped rule「一旦离开该目录就丢，再触达才重载」；Rules「压缩时 re-injected」；Skills「已调用的按共享预算 re-injected，最老的先淘汰」；Subagents「只有 final message 回主会话」；Hooks「完全绕过 compaction」；Output styles / append system prompt「永不压缩」。

### 4.1 CLAUDE.md 🏛️

- **保持 < 200 行**：太长会消耗上下文并降低指令遵从度。官方还建议**给它指定 owner、像 code 一样 review 改动**。🏛️
- **只放项目事实，不放流程**：构建命令、技术栈、目录结构、编码规范、高危模块、分支/合并约定。部署流程、review 清单、发布步骤 → 放 Skill。
- **作用域**（4 层，由远及近）：
  - 组织级（托管策略，个人不可排除）：macOS `/Library/Application Support/ClaudeCode/CLAUDE.md`，Linux `/etc/claude-code/CLAUDE.md`
  - 用户级 `~/.claude/CLAUDE.md`（本人所有项目）
  - 项目级 `./CLAUDE.md` 或 `./.claude/CLAUDE.md`（进 git，团队共享）
  - 本地级 `./CLAUDE.local.md`（gitignore，本人本项目）
- **根目录**会话开始即加载并常驻；**子目录** CLAUDE.md 仅在 Claude 读到该目录文件时按需加载（适合 monorepo）。
- 技巧：`@import`（如 `@AGENTS.md`）复用已有说明，**最多递归 4 层**；⚠️ `@import` 只是组织手段，导入文件**仍在启动时全量进上下文、不省 token**。`claudeMdExcludes` 排除无关团队文件；路径相关约束放 `.claude/rules/` 用 `paths` 前引按需加载。
- **写作红线**：指令要**具体可验证**（「用 2 空格缩进」优于「格式规范」）；不确定是否加载先 `/memory` 确认；规则互相冲突是「不遵守」的常见真因。
- **当成 prompt 调**：加 `IMPORTANT` / `YOU MUST` 强调；**每次 Claude 犯错后立即更新 CLAUDE.md**，让下次会话起点更好。👥
- **`#` 键快捷记忆**：编码时按 `#` 给 Claude 一句指令，它会自动并入对应的 CLAUDE.md。⚠️ **已被自动记忆取代**（2026-07 官方新文：Claude 现在自动保存相关记忆，见同目录 [02 笔记](./02-Claude-5-上下文工程新规.md)）。🏛️

### 4.2 Skills 🏛️

- 可重复流程，**在主线程跑**，每一步都能看到、能干预。
- 放 `.claude/skills/<名>/SKILL.md`，**遵循 agentskills.io 开放标准**；官方已把自定义 slash 命令合并进 Skills（`.claude/commands/deploy.md` 与 `.claude/skills/deploy/SKILL.md` 都生成 `/deploy`，等价）。
- **三层渐进披露**（省 token 关键）：`description` 常驻上下文（务必写清「何时使用」）→ 正文调用时才加载 → `references/` 按需读取。长资料放 Skill 几乎零成本，放 CLAUDE.md 则每次会话都烧 token。
- 常用 frontmatter：`name` / `description` / `disable-model-invocation`（`true`=仅手动触发，适合部署等有副作用的流程）/ `allowed-tools` / `context: fork`（子代理里跑、不占主上下文）/ `model`。
- **compaction 行为**：已调用的 skill 按**共享预算** re-inject，最老的先淘汰（调太多会被挤出上下文）。
- 典型：部署步骤、发布清单、某类固定重构流程。

### 4.3 Hooks 🏛️

- 在**生命周期事件**上确定性触发（文件编辑、工具调用、会话开始/结束等），注册在 `settings.json` 或 skill/agent frontmatter。
- 类型：`command` / `http` / `mcp_tool`（确定性执行）、`prompt` / `agent`（用 Claude 判断）。
- **exit code 语义（最易踩坑）**：`0`=放行（stdout 可输出 JSON 决策）；`2`=阻断（stderr 反馈给 Claude）；**`1`=不阻断**。更精细用 JSON `permissionDecision: allow/deny/ask/defer`，或 `updatedInput` 改写工具入参。
- **常用事件**：`PreToolUse`（可拦截）、`PostToolUse`、`UserPromptSubmit`、`Stop`（可阻止收工）、`SessionStart/End`、`SubagentStart/Stop`、`PreCompact/PostCompact`。
- 用途：自动格式化、**提交前跑测试/lint**、通知、日志、**阻断危险命令**。
- 上下文成本极低（配置在主窗口之外）。
- ⚠️ **安全警示**：hook 以你的完整权限跑任意 shell 命令、无确认，只信任可信来源。
- 判据 👥：「这条规则被打破你会不爽 → 用 hook（exit code 2 阻断）」，而偏好/建议 → CLAUDE.md。

### 4.4 Subagents 🏛️

- 定义在 `.claude/agents/<名>.md`，frontmatter（name/description/tools/model）+ body 当 system prompt。
- **核心价值：隔离**——独立上下文窗口，只有 final message 回主线程，指令体从不进父会话。
- 用于：深搜、日志分析、依赖审计、代码库研究、重构、测试调查、并行工作流。
- 关键字段：`description` 决定 Claude **何时自动委派**（写「改完代码后主动使用」能提高委派率）；`tools` 限定工具集（不写则继承全部）；`model` 可路由到便宜模型（探索类交给 Haiku 是官方明示的省钱手段）。
- 内置 `Explore`（只读搜索）、`Plan`（方案规划）已覆盖两大高频场景。
- **可嵌套 5 层**，动态工作流可编排**数十到上百**个后台 agent（编排计划和中间结果放在脚本变量里，不进上下文）。
- 想「在主线程一步步看、可干预」→ 用 Skill 而非 Subagent。

### 4.5 Rules（`.claude/rules/`）🏛️

- 带 `paths` 前引的 Markdown，约束**只在相关时加载**（如 `src/api/**`）。
- 当约束是「跨切面」或「只作用于部分路径」时，优先用 path-scoped rule，而非嵌套 CLAUDE.md。

### 4.6 Output styles 与 append system prompt 🏛️

- **Output styles**（`.claude/output-styles/`）：注入系统提示词，**指令遵从权重最高、永不压缩**。⚠️ 默认会**替换掉**默认软件工程助手指令（修改范围、注释、安全、测试验证习惯），Claude 会退化成「通用助手」——想保留默认 SE 行为要设 `keep-coding-instructions: true`。官方自带三个内置 style：`Proactive`（自主）/ `Explanatory`（教学）/ `Learning`（协作），多数需求不用自己维护。
- **append system prompt**（`append-system-prompt` CLI 标志）：只做**追加**、不改角色，只对当次调用生效；适合编码标准/输出格式/领域知识。⚠️ 边际递减：指令越多模型越难严格遵守，冲突时尤甚。

---

## 5. 测试与验证 🏛️👥

### 官方 TDD 流程（Anthropic 最爱）🏛️

1. 让 Claude **基于期望输入/输出写测试**，并**明确告诉它这是 TDD**（否则会为还不存在的功能造 mock 实现）。
2. 让它**跑测试、确认失败**，明确要求**此阶段别写实现**。
3. 满意后**先提交测试**（当检查点）。
4. 让它**写让测试通过的代码，且不许改测试**，迭代到全绿；此时可让**独立 subagent 验证实现没过拟合测试**。
5. 满意后提交代码。

> TDD 的本质是给 Claude 一个**外部 oracle 对齐目标**——有明确目标（测试、视觉稿、期望输出）时它表现最好。

- **AI 无法自测质量**（上下文越满判断越差），**测试是唯一精度不变的外部 oracle**。
- **Claude 有「实现优先」偏见**：先写代码再加测试 → 测试变成橡皮图章。真 TDD 必须显式强制 Red→Green→Refactor 三步提示词：
  - Red：「先写一个会失败的测试，**别写实现**，它应因函数不存在而失败。」
  - Green：「实现最小代码让测试通过，**只写够通过的代码**。」
  - Refactor：「重构提升质量，测试保持绿。」
- 插件强制：`tdd-dev`（Strict/Standard/Relaxed 三档）、`dev-workflow`。
- **每次任务都给一个验证方式**（测试用例、期望输出、截图、构建成功）——被多个来源称为「单点最高杠杆」。🏛️
- **先复现再修**：`cat error.log | claude`，让它写一个能复现 bug 的红测试，红了再修——「没有红测试的修复只是包装过的猜测」👥。
- **自己看一眼测试**，确认它在测有意义的行为，不是 `return true`。
- **验证先于声称完成**：不接受「应该能用了」，要跑的结果。

---

## 6. 代码审查 👥

- **独立上下文审查**：A 会话实现，B 会话用干净上下文审——避免带着实现时的偏见。
- **多 agent 并行审查**：逻辑 / 安全 / 边界 / 回归各一个专家，再对实际代码行为核验、去假阳性、按严重度排序。据报告：>1000 行的大 PR 有 84% 发现问题，平均 7.5 个/PR，误报 <1%。
- **警惕失败模式**：sycophantic self-review（自夸）、hallucinated package、test gaming、过度自信的「完成了」。
- **CI 自动化审查**：`claude -p` + `--allowedTools "Read,Grep,Glob"`（只读）跑 PR 审查；`anthropics/claude-code-action@v1`。

---

## 7. 权限与安全 🏛️

- 默认只读，写操作需确认；可按 `settings.json` 的 allowed-tools 平衡效率与安全。
- **AI 输出当不可信**：密钥不进 prompt，关键路径保留人工审查闸门。
- hooks 的 shell 风险（见 4.3）。
- 只装需要的 MCP / skill / hook，定期审计禁用不用的。

---

## 8. 反模式（避坑清单）👥

| 反模式 | 症状 | 对策 |
|---|---|---|
| 含糊指令（「优化一下」） | AI 被迫猜 | 写 **Goal / Context / Constraints / Done When** |
| 又长又空泛的规则文件 | 被部分忽略 | 规则要**短、硬、可验证**（自问：删掉这条 agent 会不会犯错？） |
| 一次装太多 MCP/skill/hook/多 agent | 上下文污染、互相打架 | 从最小集合开始，一层一层加 |
| 把「AI 说完成了」当证据 | 假完成 | 要验证命令 + 真实输出，diff 实际改动 |
| 跳过测试/静态分析/CI | 边界崩溃 | 测试/编译/lint/CI 仍是最后防线 |
| **架构漂移**（最深的问题） | helper/适配器/fallback 蔓延成平行主线、SoT 模糊、双面契约、前端 fallback 掩盖后端契约失败 | 按「归属→权威→契约→行为」顺序 review，而不只是「能跑吗」 |
| 只跑 happy path | 边界/校验/失败推理缺失 | 强制覆盖异常路径与边界 |

**四种最常见的「放错位置」**（规则分层落地时）：
1. 把**流程**（发布步骤、review 清单、排障）塞进 CLAUDE.md → 拆成 Skill；
2. 把**强制规则**（「永远别执行某命令」）写成提示词 → 安全/权限边界用 Hook/permissions；
3. **路径规则没写 `paths:`** → 只适用 `src/api/**` 的规则全局加载，白烧 token；
4. 把**个人偏好**（提交信息风格、语气）写进项目级文件 → 放用户级或临时 prompt。

---

## 9. 工具与命令速查

- 命令：`/init`（生成 CLAUDE.md）、`/clear`、`/compact`、`/save-session`、`/mode`（切计划/执行/思考）、`/code-review`、`/context`、`/usage`、`/plugin`。
- **思考模式层级**（提示词直接映射思考预算）：`think` < `think hard` < `think harder` < `ultrathink`——慢但减少返工，总体更快 ⚖️。
- **并行多实例 + git worktree**：同一仓库开 3–5 个隔离 worktree，各跑一个 Claude 并行推进。👥
- **屏幕截图**当输入引导/调试。🏛️
- `claude -p`（headless）接 CI/CD、`claude --permission-mode plan` 只读规划。

---

## 10. 工作流变体与多 Claude（官方）🏛️

### 视觉驱动：截图迭代

- 给 Claude 一个**截图能力**（Puppeteer MCP / iOS 模拟器 MCP / 手动粘贴）+ 一个**视觉稿**，让它实现 → 截图 → 对照 → 迭代。**迭代 2–3 轮通常远好于首版**。
- 关键：**让 Claude「看到」自己的输出**。

### Safe YOLO 模式

- `claude --dangerously-skip-permissions` 跳过所有权限检查，适合修 lint、生成样板代码。
- ⚠️ 有数据丢失 / 系统损坏 / prompt 注入外泄风险：官方建议**放进无网络容器**跑（有 Docker devcontainer 参考实现）。

### Codebase Q&A（入职/学习）

- 把 Claude 当结对同事问问题（日志怎么工作、这个端点怎么加、某行什么意思、为什么调 `foo()` 不调 `bar()`），它会 agentic 搜索作答。官方称这已是 Anthropic 的**核心 onboarding 流程**，无需特殊提示词。

### 清单 + 草稿本（复杂任务）

- 大迁移 / 修一堆 lint 错误：让 Claude **先跑命令把错误写进 Markdown 清单**，再**逐条修复 → 验证 → 勾掉**，而不是一次性全改。

### 多 Claude 并行

- **一写一审**：A 写代码，`/clear` 或另开终端，B 用干净上下文审查，再让第三个读「代码 + 反馈」并修改。分离上下文往往比单实例更好。
- **git worktree**：同仓库开 3–4 个隔离 worktree 各跑一个 Claude 做独立任务，互不等、不冲突；用完 `git worktree remove`。
- **headless fan-out / pipelining**：`claude -p` 接 CI / 预提交 / 构建。fan-out = Claude 生成任务清单 → 循环逐条 `claude -p` 执行；pipelining = `claude -p "..." --json | next_cmd`。headless **不跨会话持久**。
- **headless 当 linter / issue triage**：官方用 `claude-code-action` 做主观代码审查（typo、过时注释、误导命名）、issue-triage action 自动打标签。

## 11. 来源

**官方（Anthropic）**
- [Claude Code: Best practices for agentic coding（官方最佳实践全文镜像）](https://raw.githubusercontent.com/lwyBZss8924d/DeepSearchAgents/main/docs/VIBE/claude-code-best-practices.md) 及其中文译本 [阿里云](https://developer.aliyun.com/article/1679478)、[腾讯云](https://cloud.tencent.cn/developer/article/2562029)
- [Steering Claude Code: when to use CLAUDE.md, rules, skills, hooks, and subagents（官方博客，2026-06-18，已抓原文核对 §4）](https://claude.com/blog/steering-claude-code-skills-hooks-rules-subagents-and-more)
- [Claude Code Advanced Patterns: Subagents, MCP, and Scaling to Real Codebases（官方 PDF）](https://resources.anthropic.com/hubfs/Claude%20Code%20Advanced%20Patterns_%20Subagents,%20MCP,%20and%20Scaling%20to%20Real%20Codebases.pdf)
- [The new rules of context engineering for Claude 5 generation models（官方，2026-07-24，已抓原文，详见同目录 [02 笔记](./02-Claude-5-上下文工程新规.md)）](https://claude.com/blog/the-new-rules-of-context-engineering-for-claude-5-generation-models)
- [The Claude Code guide for startups（官方，2026-08-20，已抓原文，详见同目录 [03 笔记](./03-Claude-Code-创业公司五原则.md)）](https://claude.com/blog/claude-code-guide-for-startups)

**社区（中文详解，基于 2026-07 官方文档，已抓全文核对 §4）**
- [阿里云 — Claude Code 四大定制机制完全指南](https://developer.aliyun.com/article/1745131)
- [阿里云 — Coding Agent 规则管理：CLAUDE.md、Skills、Hooks、Subagents 到底怎么选](https://developer.aliyun.com/article/1744643)

**社区**
- [FidStyle/Vibe-Coding-Practical-Handbook — anti-patterns](https://github.com/FidStyle/Vibe-Coding-Practical-Handbook/blob/6ce3c707a4b6dd63ea9a7944b9ab6c81ddfdf9f0/8-best-practices/4-anti-patterns.md)
- [Xebia — Vibe Coding With GitHub Copilot: Fun Until Maintenance Hits](https://xebia.com/blog/vibe-coding-github-copilot-maintenance/)
- [clarity-digital-development/tworkflow（40% 上下文规则工作流）](https://github.com/clarity-digital-development/tworkflow)
- [lihuibng/claude-code-best-practice](https://github.com/lihuibng/claude-code-best-practice)
- [DataCamp — Claude Code Best Practices: Planning, Context Transfer, TDD](https://www.datacamp.com/ru/tutorial/claude-code-best-practices)
- [FlorianBruniaux/claude-code-ultimate-guide — TDD with Claude](https://deepwiki.com/FlorianBruniaux/claude-code-ultimate-guide/7.3-test-driven-development-with-claude)
- [Paradime — Claude Code Skills & Harness Engineering](https://www.paradime.io/guides/claude-code-skills-plugins-rules-guide)
- [CSDN — 使用 Claude Code 进行 Vibe Coding 编程最佳实践](https://blog.csdn.net/jjhfen00/article/details/158045319)

---

*备注：§2–§5、§10 的官方条目已通过代理抓取 [官方原文](https://www.anthropic.com/engineering/claude-code-best-practices)（Boris Cherny，2025-04-18）逐条核对；§4 已直接抓取官方《Steering Claude Code》（2026-06-18）原文核对（不再依赖二手镜像）。工具特性（命令、参数、插件名）时效性强，使用前以官方文档为准。*
