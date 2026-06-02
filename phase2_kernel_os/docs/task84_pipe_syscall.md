# Task84：pipe() syscall 雏形

## 1. 任务目标

本轮目标是新增一个最小用户态 `pipe()` syscall：

```c
int pipe(int fds[2]);
```

成功时：

- `fds[0] = pipe read fd`
- `fds[1] = pipe write fd`

当前只做教学版雏形，不实现完整 UNIX pipe。

## 2. 为什么要做这一步

Task79~Task83 已经把 pipe 拉进了 fd 体系，并且把 shell 的 pipe 接线统一到了 `fd_dup2` 路径。

但这时 pipe 主要仍是 shell 内部隐式创建的：

```text
run A | run B
```

Task84 的意义是让用户程序自己也能显式拿到一对 pipe fd，验证：

1. pipe fd 真的能被 `read/write`
2. pipe 不只是 shell 的特殊逻辑
3. fd 体系已经可以开始向用户程序暴露 pipe 资源

## 3. 修改文件

1. `include/syscall.h`
2. `include/process.h`
3. `include/user_program.h`
4. `include/fs.h`
5. `kernel/process.c`
6. `kernel/process_parts/core_helpers.inc`
7. `kernel/process_parts/redirect_pipe.inc`
8. `kernel/syscall_parts/dispatch.inc`
9. `kernel/shell_source_parts/syscalls.inc`
10. `kernel/user_program_sources/pipe_test_elf_source.c`
11. `kernel/user_program_blobs/pipe_test_elf.inc`
12. `kernel/fs_parts/embedded_and_tables.inc`
13. `readme.md`
14. `docs/phase2.md`
15. `docs/syscall.md`
16. `docs/fd.md`
17. `docs/pipe.md`
18. `docs/user_programs.md`

## 4. 实现思路

当前仍然只复用一个全局教学版 pipe buffer。

`pipe(fds)` 的最小流程是：

1. 检查 `fds` 是否为空，是否落在当前进程用户映像/用户栈范围内
2. reset 全局 pipe buffer
3. 为当前进程分配一个 `FD_PIPE_READ`
4. 为当前进程分配一个 `FD_PIPE_WRITE`
5. 把两个 fd 写回用户态 `fds[0] / fds[1]`
6. 返回 `0`

失败时返回负值，不 panic。

## 5. 核心语义

1. `fds[0]` 只能读
2. `fds[1]` 只能写
3. `write(fds[1], ...)` 进入当前教学版 pipe buffer
4. `read(fds[0], ...)` 从当前教学版 pipe buffer 读取
5. 读完后继续返回 `0` 作为 EOF
6. 写满行为沿用 Task78 的固定容量与单次提示策略

## 6. 验证命令

1. `run pipe_test`
2. `run cat /readme.txt | run wc`
3. `run cat /readme.txt | run grep MiniOS`
4. `run cat < /readme.txt | run sort > /sorted.txt`

## 7. 当前限制

1. 不支持用户态 `dup2()` syscall
2. 不支持多个独立 pipe object
3. 不支持并发 pipe
4. 不支持阻塞读写
5. 不支持多级管道
6. 不支持 fork 后共享 pipe fd

## 8. 后续方向

1. 继续清理 pipe fd 生命周期
2. 继续做用户态 `dup2()` syscall 雏形
3. 继续推进从全局 pipe buffer 到独立 pipe object
