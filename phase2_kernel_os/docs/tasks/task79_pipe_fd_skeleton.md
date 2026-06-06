# Task79：真正 pipe fd 雏形

## 1. 任务目标

Task79 的目标不是一步做到完整 UNIX pipe，而是让当前教学版 pipe 开始进入 fd 体系。

本轮希望做到：

1. fd 表能够区分普通文件 fd 和 pipe fd
2. pipe 具有最小 `read end / write end` 概念
3. `sys_read` / `sys_write` 能根据 fd 类型分发到文件或 pipe
4. `run A | run B` 内部开始通过 pipe fd 连接左右程序

## 2. 为什么现在引入 pipe fd

Task69 ~ Task78 的教学版 pipe 已经能跑通：

```text
左程序 stdout -> 全局 pipe buffer -> 右程序 stdin
```

但它本质上还是特殊路径：

1. 左边特判写 pipe
2. 右边特判读 pipe

这能演示数据流，但还不够统一。

真实操作系统更自然的模型是：

1. 用户态只看到 fd
2. `read(fd, ...)` / `write(fd, ...)` 不关心底层是文件还是 pipe
3. 内核根据 fd 背后的类型做分发

Task79 的意义，就是让 MiniOS 朝这个方向迈出第一步。

## 3. 修改文件

本轮主要修改：

1. `include/process.h`
2. `kernel/process.c`
3. `kernel/process_parts/core_helpers.inc`
4. `kernel/process_parts/fd_and_input.inc`
5. `kernel/process_parts/redirect_pipe.inc`
6. `kernel/process_parts/fork_and_reporting.inc`
7. `readme.md`
8. `docs/phase2.md`
9. `docs/pipe.md`
10. `docs/fd.md`

并新增本文档。

## 4. 实现思路

本轮仍然只保留一个全局教学版 pipe buffer，不引入多个 pipe object。

最小实现策略是：

1. 在 fd 表里增加类型字段
2. 新增：
   - `pipe read fd`
   - `pipe write fd`
3. 左侧程序执行前，为它绑定一个 `pipe write fd`
4. 右侧程序执行前，为它绑定一个 `pipe read fd`
5. `SYS_READ` 和 `SYS_FD_WRITE` 根据 fd 类型分发到文件或 pipe
6. `SYS_WRITE(stdout)` 仍保留教学版快捷路径，但最终落到绑定的 `pipe write fd`

这是一种“兼容旧路径、逐步纳入 fd 体系”的过渡设计。

## 5. 核心语义

当前 fd 类型最小包括：

1. `FD_FILE`
2. `FD_PIPE_READ`
3. `FD_PIPE_WRITE`

当前语义是：

1. `pipe read fd`
   - 只能读
   - 从当前唯一的教学版 pipe buffer 读取
   - 读完返回 `0`

2. `pipe write fd`
   - 只能写
   - 向当前唯一的教学版 pipe buffer 写入
   - 写满沿用 Task78 的容量限制和错误提示

3. `run A | run B`
   - 左侧绑定 `pipe write fd`
   - 右侧绑定 `pipe read fd`
   - 仍然是顺序执行，不是并发 UNIX pipe

## 6. 验证命令

本轮重点验证：

1. `run cat /readme.txt`
2. `run cat < /readme.txt`
3. `run cat /readme.txt > /copy.txt`
4. `run cat /readme.txt | run cat`
5. `run cat /readme.txt | run wc`
6. `run cat /readme.txt | run grep MiniOS`
7. `run cat /readme.txt | run head -n 3`
8. `run cat /readme.txt | run tail -n 3`
9. `run cat /readme.txt | run sort`
10. `run cat /readme.txt | run grep MiniOS > /grep.txt`
11. `run cat < /readme.txt | run wc`
12. `run cat < /readme.txt | run sort > /sorted.txt`

另外还需要验证连续 pipe 不会复用上一条命令的残留状态。

## 7. 当前限制

Task79 之后，pipe 已经进入 fd 体系，但仍然是教学版雏形。

当前不支持：

1. 用户态 `pipe()`
2. `dup2()`
3. fork 后共享 pipe fd
4. 阻塞读写
5. 并发 pipe
6. 多级管道
7. 多个 pipe object
8. 完整 UNIX pipe 生命周期管理

## 8. 后续方向

后续可以继续往几个方向推进：

1. 继续清理特殊字段，让 pipe 更统一地走 fd 路径
2. 引入 `dup2()` 雏形
3. 引入用户态 `pipe()` syscall 雏形
4. 让 fork 后共享 pipe fd 成为可能
5. 再往阻塞读写和真正并发 pipe 演进
