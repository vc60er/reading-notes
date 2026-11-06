# 视频资源：YouTube 与 B站

> 定位：vibe coding / Claude Code 相关的视频资源，**「行业大佬 / 官方」优先**，平台覆盖 YouTube + B站。
> ⚠️ 检索到的结果里，B站有大量标题党卖课教程（「7天速通」「少走99%弯路」），L站用户明确提醒有「噶韭菜」嫌疑——本笔记分两档标注，卖课类仅供参考、不背书。
> 整理时间：2026-08-21

---

## 一、YouTube（英文 · 官方 / 大佬，首选）

### 概念源头：Andrej Karpathy（「vibe coding」一词的提出者）

| 视频 | 讲者/出处 | 时长 | 看点 |
|---|---|---|---|
| [Software Is Changing (Again)](https://www.youtube.com/watch?v=LCEmiRjPEtQ) | Karpathy @ Y Combinator（AI Startup School keynote，2025-06） | ~39min | 「Vibe Coding: Everyone is now a programmer」独立章节在 29:06；提出 Software 3.0、自然语言即编程接口；关联他博客 [Vibe coding MenuGen](https://karpathy.bearblog.dev/vibe-coding-menugen/) |
| [From Vibe Coding to Agentic Engineering](https://www.youtube.com/watch?v=96jN2OCOfLs) | Karpathy × Sequoia Stephanie Zhan（AI Ascent 2026） | ~30min | vibe coding 正演变为更严谨的「agentic engineering」；把 LLM 比作「ghost 而非 animal」；金句 **「你可以外包思考，但不能外包理解」** |
| How I use LLMs | Karpathy（个人频道，搜标题） | — | 在 Cursor 里演示 vibe coding：描述意图 → AI 写码 → 运行 → 验证迭代；曾引发「是否还需要学编程」的争议 |

### 官方一手：Anthropic「Code with Claude」

- **出处**：Anthropic 官方 YouTube → 播放列表 **「Code w/ Claude Developer Conference」**。三场活动：旧金山（2026-05-06）、伦敦（05-19）、东京（06-10），会后约 7–10 天放出录播。
- **开场 Keynote**：[https://www.youtube.com/live/GMIWm5y90xA](https://www.youtube.com/live/GMIWm5y90xA)——发布 rate limit 上调、SpaceX 算力合作、Claude Opus 4.7、Claude Code 桌面版/SDK、CI Auto-fix、Routines。
- 另有官方 **Code with Claude 教程系列**（覆盖 Claude Code、自动化、prompting、MCP、AI 应用开发）。

### 工程实践大佬（Claude Code 深度使用者）

| 资源 | 讲者 | 看点 |
|---|---|---|
| Master All 6 Claude Code Dynamic Workflows（2026-06） | Mark Kashef（Prompt Advisers，被称「顶级 Claude Code 专家」） | 六种动态工作流模式：classify-and-act / fan-out-and-synthesize / **adversarial verification** / generate-and-filter / tournament / loop-until-done；可堆叠、可打包成 skill |
| These 5 Claude Code Hacks Run His Business（2026-07） | Mark Kashef | 「operator 而非 chatbot」心态；**多模型省成本**：Opus 规划 / Sonnet 执行 / Gemini 当廉价苦力 |
| How I Plan, Build, and Run Loops with Claude Code in 40 Minutes（podcast，2026-07） | Thariq Shihipar（Claude Code 团队，即 02 笔记作者） | `/goal` 防止提前收工；先规划消除未知再动手；Slack 里并行跑多个 Claude；**creator/verifier 双子代理**；系统提示词砍 80% |
| Claude Code Best Practice（2026-04 视频笔记） | Matt Pocock | 「smart zone」；系统提示词过大直接进「dumb zone」→ **保持极小**；多阶段 plan vs loops |
| Claude Code 101: 11 Months of Lessons in 60 mins | Avthar Sewrathan（Maven） | 36+ 条 pro-tips 与避坑 |
| Claude Code in Action（DataCamp 课程，2026-07 更新） | DataCamp | 3h / 15 视频 / 46 练习；上下文管理（称「唯一核心技能」）、plan mode、hooks、MCP、SDK |
| Claude Code Full Course | Eric（freeCodeCamp） | `/goal`、Skills、MCP、GitHub 部署 全流程 crash course |

---

## 二、B站（中文 · 搬运 + 实战）

### 大佬内容的搬运/字幕版（首选）

- **Karpathy 系列**（直接搜 BV 号）：
  - [BV1GbAFz5EZ3](https://www.bilibili.com/video/BV1GbAFz5EZ3/) — Karpathy 最新长访谈：代码智能体、自动研究、AI 的「循环」时代
  - [BV1np5J6FE3z](https://www.bilibili.com/video/BV1np5J6FE3z/) — 安德烈·卡帕西：从氛围编程到智能体工程
  - [BV1qtRuB4EFp](https://www.bilibili.com/video/BV1qtRuB4EFp/) — 从氛围编程到代理工程
- **Claude Code 之父直播写代码**：B站搜 `BV1UjdgBmEbc`（标题「不剪辑、不表演！Claude Code 之父现场硬核直播写代码」）——看官方团队怎么真实地操作，比任何教程都「原味」。

### 实战教程（⚠️ 标题党多，仅供参考）

B站上多套「2026 版 Vibe Coding 企业级实战」教程内容高度雷同，核心章节不外乎：

- 国内环境安装 / API 端点配置、CLAUDE.md（全局记忆）配置、权限与会话管理
- Claude Code + Codex + Cursor 从零构建全栈项目
- Superpowers（Agent 工程化工作流框架）解析
- MCP、Skill、SubAgent 多智能体协同；多 Claude 协作、无头模式自动化
- 「11 个技巧让 Claude Code 成功率翻倍」、安全风险与代码审查

代表 BV 号（搜即可）：`BV1oXjc6CEWK`、`BV1rvu76yEm4`、`BV1C3jt64EPS`、`BV1e7VA6vEJU`、`BV1E8NE6PEMv`。

### 独立开发者 / 社区文字资源（非视频，但常被配套推荐）

- **花叔**（独立开发者，博客园）：《Claude Code 橙皮书：从入门到精通》《Claude Code 源码解析：一份价值数十亿美元的 AI 工程蓝图》。
- **LINUX DO（L站）** 社区观点：**不必过度依赖卖课**，L站本身就是好资料；建议直接动手（Claude Code + Superpowers 先做个小玩具），边做边学。

---

## 三、看完这些视频的「避坑提醒」

1. **大佬的视频讲方法论，卖课的视频讲按钮**——想提升判断力看 Karpathy / Shihipar / Kashef；想快速上手界面看 freeCodeCamp / DataCamp 那类结构化课程。
2. **「少走99%弯路」这类标题一律降权**，内容多半是几个工具的命令罗列，且版本更新后很快过期。
3. **官方 Code with Claude 的录播是最靠谱的一手**，但偏产品发布；方法论还是要看 Karpathy 和 Claude Code 团队成员（Shihipar 等）的分享——后者正好和本目录 02/04 笔记同源。
