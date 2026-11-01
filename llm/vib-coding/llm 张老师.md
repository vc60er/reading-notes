# llm 张老师视频笔记


## command

### init
**用途：**

在当前项目下，生成 claude.md 文件。

生成的过程和内容如下：

先读取整个目录，抽离关键信息，总结到 claude.md文中。包括：项目架构，技术栈，运行方式

**claude.md**

是什么：项目的记忆文件

内容包括还可以包括，编码的基本原则，怎么做，什么可以做，什么不可以做。常用命令等

用户每次一对话中，都会携带到话的上下文中

可通过输入 #... 的方式增加记忆，比如；

todo

也可以通过自然语言，让 agent 自己想，比如：
```
update claude.md ...
```

### security-review
**用途：**
项目安全检查

todo
做了哪些检查，如何实现的。



### resume

**用途：**

恢复历史会话



### permissions

管理名的 shell 的执行权限。可以添加 allow，deny，ask这些类型的规则，支持正则

最终存放在 .claude/settings.local.json文件中

### model

todo

为什么有多个选项

### memory

todo

### context

查看上下文占用情况

### clear

清理上下文

### compact

压缩上下文

### command

是什么：自定义指令

比如，完整的自动化测试，自动化编译等

这些命令存放在 .claude/commands 目录下

### agent

子智能体，类似command，不同的是可以在对话中自动触发

触发释放是：@some-agent

### hooks

聊调过程中，自动触发的功能



## 其他

### 写好文档很重要

写文档很重要，占大部分时间，写好文档很难。有个简答的办法是，现写一个简短的项目文档，然后让 ai 来完善一下，然后你来检查，如果有问题可以手动更改

### ai 的最佳编码流程

理解文档，输出计划，按照计划执行

### 写文档，懂架构，控 ai 更重要

### 交叉验证很重要

交叉验证很重要，可大幅度提高编码的成功率，方法如下

```
I have a task: 
the other agent claims:
---
...
---
double check it if true or not 
```

将两个 agent 的输出反复喂给对方，直到他们达成一致

### cluade vs codex

codex：适合解决一次性的，难道问题，或者给出方案
cluade：支持流程自动化，适合极大提升工作效率

