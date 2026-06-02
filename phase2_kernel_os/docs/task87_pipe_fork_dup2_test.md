# Task87：用户态 pipe + fork + dup2 组合测试

## 1. 任务目标

新增一个最小用户态测试程序，验证当前 MiniOS 已经能在用户态自己组合：

1. `pipe()`
2. `fork()`
3. `dup2()`
4. `read/write`

本轮重点不是新增复杂内核机制，而是做一条接近真实 UNIX 管道模型的最小闭环验证。

## 2. 为什么现在做

Task84~Task86 已经分别补齐了：

1. 用户态 `pipe()`
2. 用户态 `dup2()`
3. fork 后 fd 继承

因此现在最自然的下一步，就是验证这些能力能否在用户态自己组合起来工作。

## 3. 修改文件

1. `include/user_program.h`
2. `include/fs.h`
3. `kernel/fs_parts/embedded_and_tables.inc`
4. `kernel/user_program_sources/pipe_fork_dup2_test_elf_source.c`
5. `kernel/user_program_blobs/pipe_fork_dup2_test_elf.inc`
6. `readme.md`
7. `docs/phase2.md`
8. `docs/fd.md`
9. `docs/process.md`
10. `docs/pipe.md`
11. `docs/user_programs.md`

## 4. 实现思路

1. 父进程先 `pipe(fds)`
2. 父进程再 `fork()`
3. 子进程执行 `dup2(fds[1], 1)`
4. 子进程通过 `write(1, ...)` 把消息写入 pipe
5. 父进程 `waitpid()`
6. 父进程执行 `dup2(fds[0], 0)`
7. 父进程通过 `read(0, ...)` 把消息读回

为了避免当前教学版 pipe 缺少并发阻塞语义，这里采用“先 wait 再 read”的顺序策略。

## 5. 核心语义

当前 `pipe_fork_dup2_test` 验证的是：

1. 用户态可以自己创建 pipe fd
2. fork 后子进程会继承 pipe fd
3. 用户态可以把 pipe write fd 复制到 `fd=1`
4. 用户态可以把 pipe read fd 复制到 `fd=0`
5. 用户态只通过 `read/write` 就能完成父子间最小数据传递

## 6. 验证命令

```text
run pipe_fork_dup2_test
run pipe_test
run dup2_test
run fork_fd_test
run cat /readme.txt | run wc
run cat < /readme.txt | run grep MiniOS > /grep.txt
run cat /readme.txt > /copy.txt
cat /copy.txt
```

## 7. 当前限制

1. 当前没有 `exec`
2. 当前不是并发阻塞 pipe
3. 当前仍然只有一个全局教学版 pipe buffer
4. 当前不是完整 UNIX pipeline
5. 当前不支持多个并发 pipe object

## 8. 后续方向

1. Task88 可继续做 pipe 读端/写端关闭语义
2. Task89 可继续探索并发 pipe / 阻塞读写
3. 后续可继续做 `fork + exec + pipe + dup2` 组合测试
