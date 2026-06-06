# Task101：Phase2 回归测试清单

## 1. 任务目标

本轮不继续新增功能，而是把 Phase2 目前已经完成的能力整理成一套稳定的回归测试文档。

目标是以后每次修改后，都能快速检查：

1. 构建是否正常
2. 启动是否正常
3. RAMFS 是否正常
4. redirect / pipe / fork / dup2 / exec 是否正常
5. `mini_pipeline` 和 shell 多级管道是否仍然可用

## 2. 为什么需要回归测试

到 Task100 为止，Phase2 主线已经基本完成。

这个阶段最重要的不再是继续堆功能，而是建立一套稳定的验收清单，避免后续小改动破坏已有数据流。

## 3. 本轮新增文档

本轮新增：

1. `docs/phase2_regression.md`
2. `docs/phase2_smoke.md`
3. `docs/tasks/task101_phase2_regression.md`

## 4. 测试覆盖范围

回归清单当前覆盖：

1. RAMFS
2. 基础用户程序
3. redirect
4. pipe
5. pipe object 多实例
6. close 语义
7. dup2
8. fork fd 继承
9. exec fd 保留
10. exec argv
11. mini_pipeline
12. shell 多级管道
13. 错误输入不 panic

## 5. 实际验证命令

本轮至少执行了：

1. `make clean`
2. `make all`
3. `timeout 5s make run`

由于当前环境以无头启动验证为主，这轮没有把所有交互式 shell 命令逐条自动回放成可见输出，因此回归文档里明确区分了：

1. smoke test
2. 完整 regression test

## 6. 当前限制

这套测试文档仍然建立在当前教学版实现之上，因此要如实承认：

1. 不是完整 POSIX
2. shell parser 仍然简化
3. signal / job control / 进程组仍未实现
4. 文件系统仍是 RAMFS
5. 目前自动化程度仍有限，交互式命令更多依赖人工复核

## 7. 后续方向

Task101 完成后，下一步更适合进入：

1. Task102：Phase2 收尾清理 / 冻结点

也就是说，Task101 的作用是把“怎么验收”固定下来，Task102 再把阶段节点收口。 
