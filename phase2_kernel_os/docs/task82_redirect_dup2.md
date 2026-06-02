# Task82：Shell 重定向迁移到 dup2 路径

## 1. 任务目标

Task82 的目标是把 Shell 的文件重定向开始迁移到 `fd_dup2` 路径。

本轮重点是：

1. `< input`
2. `> output`
3. `< input > output`

当前先只迁移文件重定向，不迁移 pipe。

## 2. 为什么要把 Shell 重定向迁移到 dup2

Shell 重定向的本质，就是把：

1. 输入文件接到 `fd=0`
2. 输出文件接到 `fd=1`

真实 Unix/Linux 常见做法是：

1. `open(input)` -> `dup2(input_fd, 0)`
2. `open(output)` -> `dup2(output_fd, 1)`

Task81 已经有了教学版内核内部 `fd_dup2`，所以 Task82 要先把文件重定向的“接线方式”迁过去。

## 3. 修改文件

本轮主要修改：

1. `kernel/process.c`
2. `kernel/process_parts/fd_and_input.inc`
3. `kernel/process_parts/redirect_pipe.inc`
4. `README.md`
5. `docs/phase2.md`
6. `docs/fd.md`
7. `docs/pipe.md`

并新增本文档。

## 4. 实现思路

本轮没有重写 shell parser，而是保留原有 shell 接口：

1. `set stdin redirect`
2. `set stdout redirect`

但把内核内部实现改成：

1. 打开输入/输出文件 fd
2. 调用 `fd_dup2(..., 0/1)`
3. 再落回现有教学版 stdin/stdout 兼容语义

这样能在不大改 shell 的前提下，把接线入口迁到 `dup2` 路径。

## 5. 核心语义

当前最小语义是：

1. `< input`
   - 先打开输入文件
   - 再 `fd_dup2(input_fd, 0)`
2. `> output`
   - 先创建或打开输出文件
   - 再 `fd_dup2(output_fd, 1)`
3. `< input > output`
   - 同时设置 `fd=0` 和 `fd=1`

当前 `pipe` 不迁移：

1. `pipe` 仍走原有兼容路径
2. `pipe + redirect` 组合本轮保持兼容行为
3. 计划留到 Task83 再继续统一

## 6. 验证命令

本轮重点验证：

1. `run cat /readme.txt`
2. `run cat < /readme.txt`
3. `run cat /readme.txt > /copy.txt`
4. `run cat < /readme.txt > /copy2.txt`
5. `run cat /readme.txt > /copy3.txt`
6. `run cat /readme.txt`
7. `run wc < /readme.txt`
8. `run grep MiniOS < /readme.txt`
9. `run head -n 3 < /readme.txt`
10. `run tail -n 3 < /readme.txt`
11. `run sort < /readme.txt`
12. `run cat /readme.txt | run wc`
13. `run cat < /readme.txt | run grep MiniOS > /grep.txt`
14. `touch /redirtest.txt`
15. `writefile /redirtest.txt hello`
16. `append /redirtest.txt world`
17. `cat /redirtest.txt`

## 7. 当前限制

当前仍然不支持：

1. 用户态 `dup2` syscall
2. 完整 POSIX open flags
3. 引用计数
4. fork 后共享 fd
5. pipe 迁移到 dup2 路径
6. 完整 UNIX shell 重定向语义

## 8. 后续方向

后续可以继续推进：

1. Task83：迁移 pipe 到 dup2 路径
2. 继续整理 shell 数据流状态清理
3. 增加用户态 `dup2` syscall
