# Task85：dup2 syscall 雏形 / 用户态 fd 重定向能力

## 1. 任务目标

把已有的内核内部 `fd_dup2(oldfd, newfd)` 继续向前推进一小步，新增用户态可调用的教学版 `dup2()` syscall。

目标接口：

```c
int dup2(int oldfd, int newfd);
```

当前只要求最小雏形：

1. 成功返回 `newfd`
2. 失败返回 `-1`
3. 复用既有内核 `fd_dup2`
4. 不实现完整 POSIX 语义

## 2. 为什么新增 dup2 syscall

Task81~Task84 已经做了这些基础能力：

1. 内核内部 `fd_dup2`
2. shell redirect 迁移到 `fd_dup2`
3. shell pipe 迁移到 `fd_dup2`
4. 用户态 `pipe()` syscall

因此当前再把 `dup2()` 暴露给用户态，就能让用户态程序开始自己实验：

1. pipe fd 的复制
2. fd 位置替换
3. 后续 `fork + pipe + dup2` 组合

## 3. 修改文件

1. `include/syscall.h`
2. `include/process.h`
3. `include/user_program.h`
4. `include/fs.h`
5. `kernel/process_parts/core_helpers.inc`
6. `kernel/syscall_parts/dispatch.inc`
7. `kernel/shell_source_parts/syscalls.inc`
8. `kernel/user_program_sources/shell_elf_source.c`
9. `kernel/user_program_sources/dup2_test_elf_source.c`
10. `kernel/user_program_blobs/dup2_test_elf.inc`
11. `kernel/fs_parts/embedded_and_tables.inc`
12. `readme.md`
13. `docs/phase2.md`
14. `docs/syscall.md`
15. `docs/fd.md`
16. `docs/pipe.md`
17. `docs/user_programs.md`

## 4. 实现思路

1. 新增 `SYS_DUP2`
2. syscall 分发里增加 `SYS_DUP2(oldfd, newfd)`
3. 新增 `process_dup2(oldfd, newfd)` 作为当前进程的公开入口
4. 该入口内部复用既有 `process_fd_dup2(...)`
5. 用户态新增最小 `dup2()` wrapper
6. 新增 `dup2_test` 程序做回归

## 5. 核心语义

当前教学版 `dup2()` 语义：

1. `oldfd` 必须是一个已打开的教学版 fd
2. `newfd` 当前支持：
   - `0`
   - `1`
   - `>= 3` 的普通教学版 fd
3. 成功返回 `newfd`
4. 失败返回 `-1`
5. `oldfd == newfd` 时返回 `newfd`
6. `newfd` 已打开时先清理再覆盖

当前 `dup2_test` 采用更稳妥的验证方案：

1. `dup2(pipe_write_fd, 5)`
2. `write(5, ...)`
3. `read(pipe_read_fd, ...)`
4. `dup2(pipe_read_fd, 6)`
5. `read(6, ...)`

这样不会直接覆盖 `stdout`，便于持续打印测试结果。

## 6. 验证命令

```text
run dup2_test
run pipe_test
run cat /readme.txt | run wc
run cat < /readme.txt | run grep MiniOS > /grep.txt
run cat < /readme.txt
run cat /readme.txt > /copy.txt
cat /copy.txt
touch /dup2test.txt
writefile /dup2test.txt hello
append /dup2test.txt world
cat /dup2test.txt
```

## 7. 当前限制

1. 不是完整 POSIX `dup2`
2. 没有引用计数
3. 没有 close-on-exec
4. 没有 fork 后 fd 共享
5. `newfd >= 3` 时当前仍然是表项复制，不共享底层 file object
6. 当前 pipe 仍然只有一个全局教学版 pipe buffer

## 8. 后续方向

1. Task86 可继续做 fork 后 fd 继承语义
2. Task87 可继续做用户态 `fork + pipe + dup2` 组合验证
3. Task88 可继续探索并发 pipe / 阻塞读写雏形
