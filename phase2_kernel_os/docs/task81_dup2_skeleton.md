# Task81：dup2 雏形 / fd 重定向统一入口

## 1. 任务目标

Task81 的目标不是直接实现完整 POSIX `dup2`，而是给当前教学版 fd 系统加一个内核内部统一入口：

```text
fd_dup2(oldfd, newfd)
```

它的作用是把一个已经打开的 fd，接到另一个指定 fd 位置上，为 shell redirect / pipe 统一接线模型做准备。

## 2. 为什么引入 dup2 雏形

在真实 Unix/Linux 里：

1. 输入重定向
   - `dup2(input_fd, 0)`
2. 输出重定向
   - `dup2(output_fd, 1)`
3. pipe 左侧
   - `dup2(pipe_write_fd, 1)`
4. pipe 右侧
   - `dup2(pipe_read_fd, 0)`

用户程序不需要知道 redirect 或 pipe 的存在，它只读取 `fd=0`、写入 `fd=1`。

MiniOS 当前还没有完整 POSIX fd 模型，所以本轮只先实现教学版内核内部雏形。

## 3. 修改文件

本轮主要修改：

1. `include/process.h`
2. `kernel/process.c`
3. `kernel/process_parts/core_helpers.inc`
4. `kernel/process_parts/redirect_pipe.inc`
5. `readme.md`
6. `docs/phase2.md`
7. `docs/fd.md`
8. `docs/pipe.md`

并新增本文档。

## 4. 实现思路

本轮采用最小实现策略：

1. `fd_dup2` 只做内核内部函数
2. `oldfd` 必须来自当前教学版 `fd_table[]`
3. `newfd` 当前支持：
   - `0`
   - `1`
   - `>= 3` 的普通 fd 槽位
4. `newfd=0/1` 时，仍然通过现有教学版 stdin/stdout 兼容字段落地
5. `pipe` 路径优先迁移到 `fd_dup2`
6. 文件型 stdin/stdout redirect 暂时保留兼容路径

这样可以先统一 pipe 的接线方式，同时避免一次性重写全部 shell redirect。

## 5. 核心语义

当前 `fd_dup2(oldfd, newfd)` 的最小语义是：

1. `oldfd` 无效
   - 返回错误
2. `newfd` 超范围
   - 返回错误
3. `oldfd == newfd`
   - 直接成功
4. `newfd` 已打开
   - 先清理再覆盖
5. `FD_FILE`
   - 复制当前最小字段
6. `FD_PIPE_READ`
   - 可接到 `fd=0`
7. `FD_PIPE_WRITE`
   - 可接到 `fd=1`

当前不是完整 POSIX 语义：

1. 不做引用计数
2. 不共享 file object
3. 不共享 offset 对象
4. `newfd=1` 绑定文件时，仍沿用教学版 stdout 文件重定向语义

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
13. `touch /dup2test.txt`
14. `writefile /dup2test.txt hello`
15. `append /dup2test.txt world`
16. `cat /dup2test.txt`

## 7. 当前限制

当前仍然不支持：

1. 用户态 `dup2` syscall
2. 用户态 `pipe()`
3. 引用计数
4. fork 后共享 fd
5. close-on-exec
6. 完整 POSIX `dup2` 语义
7. 并发安全
8. 多个 pipe object
9. 多级管道

## 8. 后续方向

后续可以继续推进：

1. 继续把文件型 stdin/stdout redirect 迁到 `fd_dup2`
2. 增加用户态 `dup2` syscall
3. 增加 `pipe()` syscall 雏形
4. 整理 fork 后 fd 继承语义
