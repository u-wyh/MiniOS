# MiniOS Phase3 计划：对照 Linux 内核机制复盘

## 1. Phase3 目标

Phase3 的重点不建议继续堆 Phase2 风格功能，而是从已经实现的教学版机制出发，对照 Linux / UNIX 的真实实现做复盘。

核心目标是：

1. 解释 MiniOS 当前机制在 Linux 中对应什么
2. 说明当前教学版实现和真实实现差在哪里
3. 把 Phase2 的代码整理成可讲述、可展示、可复习的知识主线

## 2. 为什么建议从 Linux 对照开始

到 Phase2 结束时，MiniOS 已经具备一个完整的教学版用户态数据流系统。

这时继续堆功能的收益会变低，而“对照真实内核机制”能带来更高价值：

1. 更适合复盘
2. 更适合答辩和面试
3. 更适合明确哪些地方是教学版近似实现
4. 更适合为后续真正升级设计打基础

## 3. 推荐对照主题

建议按下面这些主题展开：

1. fd table 对照 Linux `files_struct`
2. open file object 对照 Linux `struct file`
3. pipe object 对照 Linux pipe 内核对象
4. fork 对照 `copy_files`
5. exec 对照 fd 保留 / close-on-exec
6. dup2 对照 Linux fdtable 行为
7. wait / exit 对照真实进程生命周期
8. shell pipeline 对照真实 shell

## 4. 推荐路线

建议后续可以按下面的任务方向推进：

1. Task103：Linux fd table 对照 MiniOS fd table
2. Task104：Linux pipe 对照 MiniOS pipe object
3. Task105：fork / exec / dup2 对照复盘
4. Task106：Shell pipeline 对照复盘
5. Task107：Phase2 项目展示文档

## 5. 说明

这份文档是计划，不要求在 Phase3 一开始就把所有对照实现出来。

Phase3 更偏：

1. 机制对照
2. 设计复盘
3. 文档沉淀
4. 项目展示与讲述材料整理
