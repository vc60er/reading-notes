# Claude Code 官方最佳实践 · 精读笔记

> 来源：<https://code.claude.com/docs/en/best-practices>
> 性质：Anthropic 官方文档《Best practices for Claude Code》
> 整理日期：2026-08-24
> 说明：本文为结构化精读笔记，保留原文命令、代码示例与 Before/After 对照，供日常查用。

---

## 0. 核心前提（贯穿全文的一条主线）

**几乎所有最佳实践，都源于同一个约束：Claude 的上下文窗口（context window）填得很快，而随着它填满，性能会下降。**

- 上下文窗口容纳整个对话：每一条消息、Claude 读过的每个文件、每条命令输出，全部占 token。
- 一次调试会话或代码库探索，就可能产生并消耗几万 token。
- 上下文越满，LLM 越可能"忘记"前面的指令、犯更多错误。
- **因此：上下文窗口是最需要管理的资源。**

配套工具/方法：
- 用自定义 status line 持续跟踪上下文用量。
- 参考"Reduce token usage"来降低 token 消耗。

---

## 1. 给 Claude 一个验证自己工作的方式（Give Claude a way to verify its work）

**核心观点**：给 Claude 一个"能跑的通/不通检查"——测试、构建、可对比的截图。这是"需要你盯着的会话"和"可以放心走开的会话"之间的分水岭。

- Claude 在"工作看起来做完了"时就会停下来。如果没有一个能跑的检查，"看起来做完了"就成了唯一信号，**你就变成了验证闭环本身**——每个错误都要等你发现。
- 给它一个能产出"通过/失败"的东西，闭环就能自己闭合：Claude 干活 → 跑检查 → 读结果 → 迭代到通过为止。
- "检查"可以是任何能在对话里读到的信号：测试套件、构建退出码、linter、把输出和 fixture 做 diff 的脚本、和设计稿对比的浏览器截图。
- 在 Claude 的检查通过后，**自己再跑一遍 `/verify`**，对照运行中的应用确认改动。

### Before / After 对照

| 策略 | Before（模糊） | After（带验证） |
|---|---|---|
| 提供验证标准 | "implement a function that validates email addresses" | "write a validateEmail function. example test cases: user@example.com is true, invalid is false, user@.com is false. run the tests after implementing" |
| 可视化验证 UI | "make the dashboard look better" | "[粘贴截图] implement this design. take a screenshot of the result and compare it to the original. list differences and fix them" |
| 治本而非治标 | "the build is failing" | "the build fails with this error: [粘贴报错]. fix it and verify the build succeeds. address the root cause, don't suppress the error" |

### 检查的"把关强度"分四档（每一档都在"配置成本"和"需要你投入的注意力"之间做权衡）

1. **单条提示词内**：让 Claude 在同一条消息里跑检查并迭代（如上表）。
2. **跨会话**：把检查设为 `/goal` 条件。一个独立的评估器在每一轮之后重新检查，Claude 持续工作直到 goal 达成。若卡住，Claude Code 最终会在 goal 仍置位的情况下停止这次运行。
3. **确定性门禁**：用 **Stop hook** 把检查作为脚本运行，在检查通过前阻止这一轮结束。连续被阻止 8 次后，Claude Code 会覆盖 hook 并结束本轮。
4. **第二意见**：用一个 verification subagent（或会自查发现的 dynamic workflow），让一个全新的模型去尝试**反驳**结果——干活的和打分的不是同一个 agent。

- 每条路径的取舍：提示词版今天就能用在任何任务上；`/goal` 和 Stop hook 版是让你**无人值守时也能正确跑完**的手段。
- 让 Claude **出示证据**而非声称成功：测试输出、它跑的命令及返回、结果的截图。审证据比自己重跑一遍快，且对"你没盯着的会话"也有效。

---

## 2. 先探索，再计划，再编码（Explore first, then plan, then code）

**核心观点**：把"调研/规划"和"实现"分开，避免解决错误的问题。让 Claude 直接开写，可能产出解错题的代码。用 **plan mode** 把探索与执行隔离。

推荐的四阶段工作流：

| 阶段 | 操作 | 示例 |
|---|---|---|
| 1. 探索 | 按 `Shift+Tab` 直到状态栏显示 `⏸ plan mode on`，或用 `claude --permission-mode plan` 启动。Claude 只读文件、回答问题、不改动。 | `read /src/auth and understand how we handle sessions and login. also look at how we manage environment variables for secrets.` |
| 2. 计划 | 让 Claude 产出详细实现计划。按 `Ctrl+G` 可在文本编辑器里直接改计划。 | `I want to add Google OAuth. What files need to change? What's the session flow? Create a plan.` |
| 3. 实现 | 批准计划或按 `Shift+Tab` 退出 plan mode，让 Claude 对照计划编码。 | `implement the OAuth flow from your plan. write tests for the callback handler, run the test suite and fix any failures.` |
| 4. 提交 | 让 Claude 用描述性信息提交并建 PR。 | `commit with a descriptive message and open a PR` |

**何时跳过计划**：plan mode 有用，但也有额外开销。范围清晰、改动很小的任务（改错字、加一行日志、重命名变量）直接让它干。计划最有用的是：**对方案不确定**、**改动跨多个文件**、**对要改的代码不熟悉**。**如果一句话就能描述清楚 diff，就跳过计划。**

---

## 3. 在提示词里提供精准上下文（Provide specific context）

**核心观点**：指令越精准，需要的返工越少。Claude 能推断意图，但读不了你的心。要引用具体文件、说明约束、指向范例。

### Before / After 对照

| 策略 | Before（模糊） | After（精准） |
|---|---|---|
| 圈定范围 | "add tests for foo.py" | "write a test for foo.py covering the edge case where the user is logged out. avoid mocks." |
| 指向来源 | "why does ExecutionFactory have such a weird api?" | "look through ExecutionFactory's git history and summarize how its api came to be" |
| 参考已有模式 | "add a calendar widget" | "look at how existing widgets are implemented on the home page to understand the patterns. HotDogWidget.php is a good example. follow the pattern to implement a new calendar widget that lets the user select a month and paginate forwards/backwards to pick a year. build from scratch without libraries other than the ones already used in the codebase." |
| 描述症状 | "fix the login bug" | "users report that login fails after session timeout. check the auth flow in src/auth/, especially token refresh. write a failing test that reproduces the issue, then fix it" |

**反向提示**：模糊提示词在"探索、且能承受反复纠偏"时也有用，比如 `"what would you improve in this file?"` 能挖出你自己没想到要问的东西。

---

## 4. 提供富内容（Provide rich content）

把富数据喂给 Claude 的几种方式：

- 用 `@` 引用文件，而不是描述代码在哪（Claude 会在回应前读取该文件）。
- **直接粘贴图片**：复制/拖拽图片进提示词。
- 给文档和 API 参考的 **URL**；用 `/permissions` 把常用域名加入白名单。
- **管道灌数据**：`cat error.log | claude` 直接把文件内容发进去。
- **让 Claude 自己去取**：用 Bash 命令、MCP 工具或读文件，让 Claude 自己拉取所需上下文。

---

## 5. 配置你的环境（Configure your environment）

几个设置步骤能让所有会话显著更有效。

### 5.1 写一份有效的 CLAUDE.md

- 用 `/init` 基于当前项目结构生成一份 starter `CLAUDE.md`，再逐步精修。
- `CLAUDE.md` 是每个会话**开始时** Claude 都会读的特殊文件。放 Bash 命令、代码风格、工作流规则——这些是光看代码推不出来的**持久上下文**。
- 无强制格式，但**保持简短、人类可读**。示例：

```markdown
# Code style
- Use ES modules (import/export) syntax, not CommonJS (require)
- Destructure imports when possible (eg. import { foo } from 'bar')

# Workflow
- Be sure to typecheck when you're done making a series of code changes
- Prefer running single tests, and not the whole test suite, for performance
```

- 用 `/context` 确认 Claude 确实加载了该文件。
- 只放**普遍适用**的东西；领域知识或偶尔才用的工作流 → 用 **skills**（按需加载，不撑爆每次对话）。
- **保持精炼**：对每一行问一句"删掉它会不会让 Claude 犯错？"不会就删。**臃肿的 CLAUDE.md 会让 Claude 忽略你真正的指令！**

#### 该放 / 不该放对照

| ✅ 该放 | ❌ 不该放 |
|---|---|
| Claude 猜不出来的 Bash 命令 | Claude 读代码就能搞懂的 |
| 与默认值不同的代码风格规则 | Claude 已知的标准语言惯例 |
| 测试说明、偏好的 test runner | 详细 API 文档（改为链接） |
| 仓库礼仪（分支命名、PR 约定） | 频繁变动的信息 |
| 项目特有的架构决策 | 长篇解释或教程 |
| 开发环境怪癖（必需的 env vars） | 逐文件的代码库描述 |
| 常见坑 / 非显然行为 | 不言自明的话，如"write clean code" |

#### 维护要点

- 若 Claude **总做你不想让它做的事**（哪怕已有规则禁止）→ 文件太长，规则被淹没了。
- 若 Claude **问的问题答案已在 CLAUDE.md 里** → 措辞可能有歧义。
- **把 CLAUDE.md 当代码**：出错时 review、定期删减、通过观察 Claude 行为是否真的变了来测试改动。
- 已纳入 git 的 CLAUDE.md，可跑 `/doctor` 让 Claude 提议删掉那些它本可从代码库里推出来的内容。
- 若 Claude 反复跳过某条指令，**只对那一行**加 `IMPORTANT` 强调；如果强调了很多行，就都不突出了。
- **提交到 git**，团队可共同贡献；文件价值随时间复利。
- CLAUDE.md 可用 `@path/to/import` 语法导入额外文件。

### 5.2 配置权限（Configure permissions）

- 想要更少打断又不失控：用 `/permissions` 预批准你信任的工具；用 `/sandbox` 让沙箱命令免询问运行。想自己审批每次编辑/命令时切到 **Manual mode**。
- **Pro/Max/Team 套餐**：交互终端与 VS Code 会话默认是 **auto mode**——由一个分类器模型代你审查大多数动作，只拦看起来有风险的（如权限升级、未知基础设施、恶意内容驱动的动作）。
- **其他套餐**默认 **Manual mode**：Claude 在可能改动系统的动作前（文件写、Bash 命令、MCP 工具）会询问——安全但繁琐，第十次批准之后你就是在机械点击而非审查了。两个工具能砍掉打断（auto mode 同样适用）：
  - **权限白名单**：放行你确定安全的工具，如 `npm run lint`、`git commit`。
  - **沙箱化**：启用 OS 级隔离，限制文件系统和网络访问，让 Claude 在划定边界内更自由地干活。

### 5.3 使用 CLI 工具（Use CLI tools）

- 与外部服务交互时，让 Claude 用 CLI：`gh`、`aws`、`gcloud`、`sentry-cli`。
- **CLI 是与外部服务交互最省上下文的方式**。用 GitHub 就装 `gh`——Claude 会用 `gh` 建 issue、开 PR、读评论。没有 `gh`，Claude 也能用 GitHub API，但未认证请求常触达限流。
- Claude 也擅长现学不会的 CLI。试这种提示：`Use 'foo-cli-tool --help' to learn about foo tool, then use it to solve A, B, C.`

### 5.4 连接 MCP 服务器（Connect MCP servers）

- 用 `claude mcp add` 加服务器名和 URL/命令，接入 Notion、Figma、数据库等外部工具。例：

```bash
claude mcp add --transport http notion https://mcp.notion.com/mcp
```

- 有了 MCP 服务器，可以让 Claude：从 issue 跟踪器实现功能、查数据库、分析监控数据、集成 Figma 设计、自动化工作流。

### 5.5 设置 hooks（Set up hooks）

- 用于**每次都必须发生、零例外**的动作。
- hooks 在 Claude 工作流的特定节点自动跑脚本。与 `CLAUDE.md` 的"建议性"不同，**hooks 是确定性的**，保证动作一定发生。
- Claude 可以帮你写 hooks。试："Write a hook that runs eslint after every file edit" 或 "Write a hook that blocks writes to the migrations folder."
- 直接编辑 `.claude/settings.json` 手动配置，用 `/hooks` 浏览已配置内容。

### 5.6 创建 skills（Create skills）

- 在 `.claude/skills/` 下建 `SKILL.md`，给 Claude 领域知识和可复用工作流。
- skills 用你项目/团队/领域特有的信息扩展 Claude 的知识；相关时自动应用，或用 `/skill-name` 直接调用。

示例一（领域约定）：

```markdown
.claude/skills/api-conventions/SKILL.md
---
name: api-conventions
description: REST API design conventions for our services
---
# API Conventions
- Use kebab-case for URL paths
- Use camelCase for JSON properties
- Always include pagination for list endpoints
- Version APIs in the URL path (/v1/, /v2/)
```

示例二（可复用的工作流，手动触发）：

```markdown
.claude/skills/fix-issue/SKILL.md
---
name: fix-issue
description: Fix a GitHub issue
disable-model-invocation: true
---
Analyze and fix the GitHub issue: $ARGUMENTS.

1. Use `gh issue view` to get the issue details
2. Understand the problem described in the issue
3. Search the codebase for relevant files
4. Implement the necessary changes to fix the issue
5. Write and run tests to verify the fix
6. Ensure code passes linting and type checking
7. Create a descriptive commit message
8. Push and create a PR
```

- 用 `/fix-issue 1234` 调用。
- 对有副作用、希望手动触发的工作流，加 `disable-model-invocation: true`。

### 5.7 创建自定义 subagents（Create custom subagents）

- 在 `.claude/agents/` 里定义专门助手，Claude 可委派隔离任务。
- subagents 跑在**自己的上下文**里，有**自己允许的工具集**。适合读很多文件、或需要专注而别污染主对话的任务。

```markdown
.claude/agents/security-reviewer.md
---
name: security-reviewer
description: Reviews code for security vulnerabilities
tools: Read, Grep, Glob, Bash
model: opus
---
You are a senior security engineer. Review code for:
- Injection vulnerabilities (SQL, XSS, command injection)
- Authentication and authorization flaws
- Secrets or credentials in code
- Insecure data handling

Provide specific line references and suggested fixes.
```

- 显式让 Claude 用：`Use a subagent to review this code for security issues.`

### 5.8 安装插件（Install plugins）

- 用 `/plugin` 浏览市场。插件把 skills、hooks、subagents、MCP 服务器打包成一个可安装单元（来自社区和 Anthropic）。
- 用强类型语言的话，装一个 **code intelligence 插件**，给 Claude 精确的符号导航和编辑后自动错误检测。
- 在 skills / subagents / hooks / MCP 之间如何选，见"Extend Claude Code"。

---

## 6. 高效沟通（Communicate effectively）

问 Claude 你会问另一个工程师的问题；更大的功能，先让 Claude 面试你、写一份 spec 再动手实现。

### 6.1 问代码库问题（Ask codebase questions）

把 Claude 当资深工程师来问，用于上手新代码库、学习探索。直接问即可，无需特殊提示：

- "How does logging work?"
- "How do I make a new API endpoint?"
- "What does `async move { ... }` do on line 134 of `foo.rs`?"
- "What edge cases does `CustomerOnboardingFlowImpl` handle?"
- "Why does this code call `foo()` instead of `bar()` on line 333?"

这是有效的 onboarding 工作流，缩短上手时间、减轻其他工程师负担。

### 6.2 让 Claude 采访你（Let Claude interview you）

- 更大的功能，先让 Claude 面试你。用一个最小提示，让它用 `AskUserQuestion` 工具提问——它会问到你还没想到的东西（技术实现、UI/UX、边界情况、权衡）。
- 把 `[brief description]` 换成你的功能再发：

```
I want to build [brief description]. Interview me in detail using the AskUserQuestion tool.

Ask about technical implementation, UI/UX, edge cases, concerns, and tradeoffs. Don't ask obvious questions, dig into the hard parts I might not have considered.

Keep interviewing until we've covered everything, then write a complete spec to SPEC.md.
```

- spec 完成后，**开一个全新会话去执行**——新会话上下文干净、专注实现，且你有一份可引用的书面 spec。
- 最有用的 spec 是**自包含**的：点名涉及的文件和接口、说明 out of scope、并以一个能证明功能可用的端到端验证步骤收尾。**把 spec 写精确花的时间，比盯实现花的时间更值。**

---

## 7. 管理你的会话（Manage your session）

会话是**持久且可逆**的——善用这一点。

### 7.1 尽早、经常纠偏（Course-correct early and often）

一发现 Claude 走偏就纠正。最好的结果来自**紧密的反馈环**；即使 Claude 偶尔一次就完美解决，快速纠正通常能更快产出更好的方案。

- `Esc`：中途停下 Claude，上下文保留，可重定向。
- `Esc + Esc` 或 `/rewind`：打开 rewind 菜单，恢复之前的对话和代码状态，或从选中消息开始总结。
- `"Undo that"`：让 Claude 撤销改动。
- `/clear`：无关任务之间重置上下文；带着无关上下文的漫长会话会降低性能。

> 若同一问题上你**纠正 Claude 超过两次**，说明上下文已被失败尝试污染。跑 `/clear`，用一句**吸收了你学到的东西**的更精准提示重新开始。干净的会话 + 更好的提示，几乎总胜过累积纠正的长会话。

### 7.2 激进地管理上下文（Manage context aggressively）

- 无关任务之间跑 `/clear` 重置上下文。
- 接近上下文上限时，Claude Code 会**自动压缩**（compact）对话历史，保留重要代码和决策、释放空间。
- 长会话中上下文会塞满无关对话、文件内容、命令，降低性能甚至让 Claude 分心。手段：
  - 任务之间频繁 `/clear` 彻底重置。
  - 自动压缩触发时，Claude 会总结最重要的东西（代码模式、文件状态、关键决策）。
  - 更多控制：`/compact <instructions>`，如 `/compact Focus on the API changes`。
  - 只压缩部分对话：`Esc + Esc` 或 `/rewind` 选消息检查点，选 **Summarize from here**（压缩该点之后的，保留之前的）或 **Summarize up to here**（压缩之前的，保留最近的完整）。
  - 在 CLAUDE.md 里定制压缩行为，如 `"When compacting, always preserve the full list of modified files and any test commands"`。
  - 不需要留在上下文里的问题用 `/btw`：答案**不进**对话历史，可查细节而不增上下文。

### 7.3 用 subagents 做调查（Use subagents for investigation）

- 用 `"use subagents to investigate X"` 委派调研。它们**在独立上下文里探索**，主对话保持干净留给实现。
- 上下文是根本约束，subagents 是最有力的工具之一：Claude 研究代码库要读大量文件，全占你的上下文；subagents 在独立上下文里跑，只回报摘要。
- 例：`Use subagents to investigate how our authentication system handles token refresh, and whether we have any existing OAuth utilities I should reuse.`
- 也可用于实现后的验证（见 8.5 对抗式审查）。

### 7.4 用检查点回退（Rewind with checkpoints）

- 你发的**每条提示都创建检查点**，可把对话/代码/两者恢复到任意历史检查点。
- Claude 在每次改动前自动快照文件，所以检查点能恢复它们。双击 `Esc` 或 `/rewind` 打开菜单：可只恢复对话、只恢复代码、恢复两者、或从某条消息总结。
- 不必精心规划每一步——可让 Claude 试危险操作，不行就 rewind 换方案。
- 检查点随对话保存，可关终端、之后恢复会话、仍能 rewind。
- **局限**：检查点只追踪通过 Claude 文件编辑工具做的改动；Bash 命令或外部进程改的不算。**它不是 git 的替代品。**

### 7.5 恢复会话（Resume conversations）

- 用 `/rename` 给会话命名，像对待分支一样：每个工作流各有一份持久上下文。
- 会话本地保存，跨多次任务无需重讲上下文。`claude --continue` 接着上次，`claude --resume` 从列表选。起描述性名字如 `oauth-migration`，方便日后找到。

---

## 8. 自动化与规模化（Automate and scale）

能高效用好一个 Claude 之后，用并行会话、非交互模式、fan-out 模式成倍放大产出。

### 8.1 非交互模式（Run non-interactive mode）

- 在 CI、pre-commit hook、脚本里用 `claude -p "prompt"`。加 `--output-format stream-json --verbose` 输出流式 JSON。
- 非交互运行仍会创建可恢复的会话，除非传 `--no-session-persistence`。
- 输出格式支持程序化解析：纯文本、JSON、流式 JSON。

```bash
# 一次性查询
claude -p "Explain what this project does"

# 结构化输出（供脚本用）
claude -p "List all API endpoints" --output-format json

# 实时流式处理
claude -p "Analyze this log file" --output-format stream-json --verbose
```

- 第一条打印纯文本；`json` 返回带 `result` 字段的单个 JSON 对象；`stream-json` 每行一个 JSON 对象，从 init 事件开始。

### 8.2 跑多个 Claude 会话（Run multiple Claude sessions）

- 并行跑多个会话，加速开发、跑隔离实验、或启动复杂工作流。
- 按"你想自己做多少协调"选并行方式：
  - **Worktrees**：在隔离的 git checkout 里跑独立 CLI 会话，编辑不冲突。
  - **桌面 App**：可视化管理多个本地会话，各自在自己的 worktree。
  - **Claude Code on the web**：云端跑会话，默认在 Anthropic 托管的基础设施上。
  - **Agent view**（research preview）：`claude agents` 派发后台持续运行的会话，一屏看多个。
  - **Agent teams**（实验性，默认关闭）：多会话自动协调，共享任务、消息、有 team lead。
- 并行之外，多会话还能做**质量向**工作流：全新上下文能让代码审查更客观——Claude 不会偏袒它刚写的代码。
- **Writer / Reviewer 模式**示例：

| Session A（Writer） | Session B（Reviewer） |
|---|---|
| Implement a rate limiter for our API endpoints | Review the rate limiter implementation in @src/middleware/rateLimiter.ts. Look for edge cases, race conditions, and consistency with our existing middleware patterns. |
| Here's the review feedback: [Session B output]. Address these issues. | |

- 测试也同理：一个 Claude 写测试，另一个写代码去通过它们。

### 8.3 跨文件 fan-out（Fan out across files）

- 对每个任务循环调 `claude -p`；用 `--allowedTools` 为批量操作圈定权限。
- 大型迁移/分析，可把工作分发到很多并行调用。在 git 仓库里跑 `/batch <instruction>`，让 Claude 把改动拆给 **5–30 个 subagents**，各自在自己的 worktree 里干活并开 PR。
- 自己写脚本驱动 fan-out，则循环 `claude -p`，三步：

1. **生成任务清单**：让 Claude 把需迁移的文件列表写进文件，下一步循环读它。提示如 `list all 2,000 Python files that need migrating and save the list to files.txt`。
2. **写循环脚本**：

```bash
for file in $( cat files.txt ); do
  claude -p "Migrate $file from React to Vue. Return OK or FAIL." \
    --allowedTools "Edit,Bash(git commit *)"
done
```

3. **先在小样上试，再规模化**：先用前 2–3 个文件打磨提示，再跑全集。`--allowedTools` 限制 Claude 能做什么——无人值守时尤其重要。

- 也可接入现有数据/处理流水线：`claude -p "<your prompt>" --output-format json | your_command`。
- 开发期用 `--verbose` 调试，生产关掉。

### 8.4 用 auto mode 自主运行（Run autonomously with auto mode）

- 想要不中断执行 + 后台安全检查，用 auto mode：分类器模型在命令运行前审查，拦权限升级、未知基础设施、恶意内容驱动的动作，放行常规工作。

```bash
claude --permission-mode auto -p "fix all lint errors"
```

- 当分类器在带 `-p` 的非交互运行里反复拦截动作时，Claude Code **不会停止这次运行**（见"when auto mode falls back"了解具体行为与阈值）。

### 8.5 加一步对抗式审查（Add an adversarial review step）

- 在把任务当"完成"之前，让一个 subagent 在**全新上下文**里审查 diff 并回报缺口。
- 越长无人值守，独立检查越重要。reviewer 跑在全新 subagent 上下文里，**只看到 diff 和你给的标准**，看不到产生改动的推理过程，因此能独立评估结果。
- 正确性检查：跑内置的 `/code-review` skill——在全新 subagent 里审查当前 diff 找 bug，把发现返回会话。
- 对照计划审查 diff，就自己写审查提示（点名要查的活、对照的计划、什么算 finding）：

```
Use a subagent to review the rate limiter diff against PLAN.md. Check that
every requirement is implemented, the listed edge cases have tests, and
nothing outside the task's scope changed. Report gaps, not style preferences.
```

- reviewer 作为 subagent 运行，实现会话能直接收到缺口、修复并再审，无需你在窗口间手动搬运发现。
- **注意**：让 reviewer 找缺口，它通常会报一些——即使工作其实没问题，因为那就是它被要求做的。逐条追会导致**过度工程**：多余的抽象层、防御性代码、为不可能发生的场景写测试。**让 reviewer 只标记影响正确性或明确需求的缺口，其余当可选。**

---

## 9. 避免常见失败模式（Avoid common failure patterns）

| 失败模式 | 症状 | 解法 |
|---|---|---|
| **大杂烩会话（kitchen sink session）** | 从一个任务开始，又插进无关问题，再回到原任务；上下文塞满无关信息 | 无关任务之间 `/clear` |
| **反复纠正** | Claude 做错 → 纠正 → 还错 → 再纠正；上下文被失败尝试污染 | 失败纠正两次后 `/clear`，写一条吸收教训的更好初始提示 |
| **过度规定的 CLAUDE.md** | 太长，Claude 忽略一半，重要规则淹没在噪声里 | 狠心删减；若没有这条指令 Claude 也能做对，就删掉或改成 hook |
| **先信任后验证的缺口** | Claude 产出看着合理、但不处理边界情况的实现 | 永远提供验证（测试/脚本/截图）；不能验证就别上线 |
| **无限探索** | 让 Claude 去"调查"却不圈范围，读几百个文件塞满上下文 | 窄化调查范围，或用 subagents 让探索不耗主上下文 |

---

## 10. 培养你的直觉（Develop your intuition）

- 本文的模式**不是铁律**，是普适但未必处处最优的起点。
- 有时该让上下文**累积**——你正深陷一个复杂问题，历史很有价值；有时该**跳过计划**直接让 Claude 试——任务偏探索性；有时**模糊提示正好**——你想先看 Claude 如何理解问题再约束它。
- **留心什么有效**：Claude 产出很好时，注意你做了什么（提示结构、给的上下文、所处的 mode）；挣扎时问为什么（上下文太吵？提示太模糊？任务太大一次做不完？）。
- 久而久之你会练出任何指南都写不出的直觉：何时具体、何时开放，何时计划、何时探索，何时清上下文、何时累积。

---

## 附：相关资源（Related resources）

- **How Claude Code works**：agentic loop、工具、上下文管理。
- **Extend Claude Code**：skills、hooks、MCP、subagents、plugins。
- **Common workflows**：调试、测试、PR 等的分步 recipe。
- **CLAUDE.md**：存项目约定与持久上下文。

---

*（本文由官方英文文档精读整理，命令/代码示例保留原文以确保准确。）*
