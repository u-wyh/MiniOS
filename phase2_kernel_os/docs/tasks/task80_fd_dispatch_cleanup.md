# Task80：fd 抽象整理 / 统一 file fd 与 pipe fd 分发路径

## 1. 任务目标

Task80 不是新增功能任务，而是结构整理任务。

本轮目标是：

1. 把当前 `fd` 类型定义写清楚
2. 把 `fd` 查询、分配、重置路径整理出来
3. 让 `sys_read / sys_write` 的 file fd 与 pipe fd 分发更清楚
4. 在不破坏现有 redirect / pipe / RAMFS 行为的前提下，减少过渡性重复判断

## 2. 为什么要整理 fd 分发

Task79 已经让 pipe 进入了 fd 体系，但当时仍然带着一些过渡实现：

1. `stdin/stdout` 还保留特殊入口
2. pipe 相关状态既有 fd，又有兼容字段
3. `read/write` 路径里还存在分散判断

如果这一层不先整理清楚，后面继续做：

1. `dup2`
2. 用户态 `pipe()`
3. fork 后 fd 继承

就会越来越难讲清楚。

## 3. 修改文件

本轮主要修改：

1. `include/process.h`
2. `kernel/process.c`
3. `kernel/process_parts/core_helpers.inc`
4. `kernel/process_parts/fd_and_input.inc`
5. `kernel/process_parts/redirect_pipe.inc`
6. `readme.md`
7. `docs/phase2.md`
8. `docs/fd.md`
9. `docs/pipe.md`

并新增本文档。

## 4. 实现思路

本轮没有重做整个 fd 子系统，而是做最小整理：

1. 增加统一 fd 查询辅助函数
2. 增加统一 fd 槽位重置辅助函数
3. 普通文件 fd 与 pipe fd 尽量复用同一套 fd table 查找路径
4. `fd=0 / fd=1` 的教学版特殊入口先保留
5. pipe stdin / stdout 尽量通过绑定的 pipe fd 进入统一逻辑

这样做的目的，是让行为不变，但结构更像后续可扩展的 fd 抽象。

## 5. 核心语义

当前 fd 类型仍然最少包括：

1. `FD_FILE`
2. `FD_PIPE_READ`
3. `FD_PIPE_WRITE`

当前 `sys_read` 语义：

1. `fd=0`
   - 先检查当前进程是否绑定了 `pipe read fd`
   - 否则再看 stdin 文件重定向
2. `FD_FILE`
   - 走普通文件读取
3. `FD_PIPE_READ`
   - 走 pipe buffer 读取
4. `FD_PIPE_WRITE`
   - 返回错误

当前 `sys_write` 语义：

1. `SYS_WRITE(text)`
   - 保留 stdout 特殊入口
   - 若 stdout 已绑定 `pipe write fd`，则写 pipe
   - 若 stdout 已重定向到文件，则写 RAMFS
   - 否则输出屏幕
2. `SYS_FD_WRITE(fd, ...)`
   - `FD_FILE` 写普通文件
   - `FD_PIPE_WRITE` 写 pipe
   - `FD_PIPE_READ` 返回错误

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
13. `touch /fdtest.txt`
14. `writefile /fdtest.txt hello`
15. `append /fdtest.txt world`
16. `cat /fdtest.txt`

## 7. 当前限制

这轮整理之后，fd 分发更清楚了，但仍然不是完整 Unix/Linux fd 模型。

当前不支持：

1. `dup2`
2. 用户态 `pipe()`
3. fork 后共享 fd
4. fd 引用计数
5. 阻塞 pipe
6. 并发 pipe
7. 多个 pipe object
8. 多级管道

## 8. 后续方向

后续可以继续推进：

1. Task81：`dup2` 雏形
2. pipe syscall 雏形
3. shell 数据流路径继续整理
4. 逐步减少兼容字段，让 pipe 更统一地走 fd 抽象
