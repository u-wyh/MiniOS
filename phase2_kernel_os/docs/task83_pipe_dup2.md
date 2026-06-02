# Task83：pipe 迁移到 dup2 路径

## 1. 任务目标

本轮目标是把 shell 的教学版单管道连接统一到 `fd_dup2` 路径：

- 左侧程序通过 `fd_dup2(pipe_write_fd, 1)` 接到 pipe 写端
- 右侧程序通过 `fd_dup2(pipe_read_fd, 0)` 接到 pipe 读端

这一步不实现完整 UNIX pipe，只整理当前顺序 pipe 的接线模型。

## 2. 为什么要做这一步

Task82 已经把文件重定向 `<` / `>` 开始迁到 `fd_dup2` 路径。

如果 pipe 仍然保持另一套单独描述方式，那么 shell 数据流会继续分裂成：

1. 文件重定向一套
2. pipe 一套

Task83 的作用就是让两者都开始共享“把某个 I/O 对象接到 `fd=0/1`”的解释方式。

## 3. 修改文件

1. `include/process.h`
2. `kernel/shell_source_parts/pipe_exec.inc`
3. `kernel/process_parts/redirect_pipe.inc`
4. `readme.md`
5. `docs/phase2.md`
6. `docs/fd.md`
7. `docs/pipe.md`

## 4. 实现思路

当前教学版 pipe 仍然只有一个全局 pipe buffer。

shell 在执行 `run A | run B` 时：

1. 先 `pipe reset`
2. 启动左侧子进程
3. 为左侧子进程分配 `pipe write fd`
4. 对左侧子进程执行 `fd_dup2(pipe_write_fd, 1)`
5. 运行左侧程序，把输出写入 pipe buffer
6. 启动右侧子进程
7. 为右侧子进程分配 `pipe read fd`
8. 对右侧子进程执行 `fd_dup2(pipe_read_fd, 0)`
9. 运行右侧程序，从 pipe buffer 读取直到 EOF
10. 最后再次 `pipe reset`

## 5. 核心语义

1. 左侧 `fd=1` 现在优先绑定到 `pipe write fd`
2. 右侧 `fd=0` 现在优先绑定到 `pipe read fd`
3. pipe 读完后继续返回 `0` 作为 EOF
4. pipe 写满时继续沿用 Task78 的固定容量与单次提示策略
5. 当前仍然是“左侧先跑完、右侧再读取”的顺序 pipe

## 6. 验证命令

1. `run cat /readme.txt | run cat`
2. `run cat /readme.txt | run wc`
3. `run cat /readme.txt | run grep MiniOS`
4. `run cat /readme.txt | run head -n 3`
5. `run cat /readme.txt | run tail -n 3`
6. `run cat /readme.txt | run sort`
7. `run cat /readme.txt | run grep MiniOS > /grep.txt`
8. `run cat < /readme.txt | run wc`
9. `run cat < /readme.txt | run sort > /sorted.txt`

## 7. 当前限制

1. 不支持用户态 `pipe()` syscall
2. 不支持用户态 `dup2()` syscall
3. 不支持 fork 后共享 pipe fd
4. 不支持阻塞读写
5. 不支持并发 pipe
6. 不支持多级管道
7. 不支持多个 pipe object

## 8. 后续方向

1. 继续清理 `fd=0/1` 的教学版特殊入口
2. 继续减少 pipe 兼容字段的参与度
3. 继续推进用户态 `dup2()` / `pipe()` syscall 雏形
