# Task90：用户态 pipeline demo / pipe + fork + dup2 + exec 端到端演示

## 1. 任务目标

Task90 的目标不是重写 shell pipeline，而是在用户态新增一个最小 demo，验证当前 MiniOS Phase2 已经能自己组合：

1. `pipe()`
2. `fork()`
3. `dup2()`
4. `exec()`

形成一条最小 `producer | consumer` 链路。

## 2. 为什么做这一步

Task84 到 Task89 已经分别补齐了：

1. `pipe()` syscall
2. `dup2()` syscall
3. `fork()` 后 fd 继承
4. `exec()` 后 fd table 保留

Task90 的意义是第一次在用户态把这些能力真正串起来，说明当前 MiniOS 的 fd 抽象、pipe fd、fork/exec 路径已经能支撑一个教学版 pipeline 骨架。

## 3. 修改文件

本轮主要修改：

1. `include/user_program.h`
2. `include/fs.h`
3. `kernel/fs_parts/embedded_and_tables.inc`
4. `kernel/user_program_sources/pipeline_demo_elf_source.c`
5. `kernel/user_program_sources/pipeline_writer_elf_source.c`
6. `kernel/user_program_sources/pipeline_reader_elf_source.c`
7. `kernel/user_program_blobs/pipeline_demo_elf.inc`
8. `kernel/user_program_blobs/pipeline_writer_elf.inc`
9. `kernel/user_program_blobs/pipeline_reader_elf.inc`
10. `readme.md`
11. `docs/phase2.md`
12. `docs/fd.md`
13. `docs/process.md`
14. `docs/pipe.md`
15. `docs/user_programs.md`

## 4. 实现思路

当前采用教学版顺序模型，而不是完整并发 pipeline：

1. `pipeline_demo` 先 `pipe(fds)`
2. `fork()` writer 子进程
3. writer 子进程 `dup2(fds[1], 1)` 后 `exec(pipeline_writer)`
4. 父进程 `waitpid(writer)`
5. 再 `fork()` reader 子进程
6. reader 子进程 `dup2(fds[0], 0)` 后 `exec(pipeline_reader)`
7. 父进程 `waitpid(reader)`
8. 最后输出 `pipeline_demo: ok`

选择顺序模型的原因是：

1. 当前只有一个全局教学版 pipe buffer
2. 当前没有阻塞读写
3. 当前不追求完整 UNIX pipeline 并发语义

## 5. 核心语义

当前 demo 的核心语义是：

1. `pipeline_writer` 不关心 `fd=1` 背后是什么，只负责 `write(1, ...)`
2. `pipeline_reader` 不关心 `fd=0` 背后是什么，只负责 `read(0, ...)`
3. `dup2` 负责把 pipe 端点接到标准输入输出
4. `exec` 后这些 fd 绑定关系仍然保留

所以这个 demo 验证的是：

1. 标准输入输出已经能作为可被重新接线的 fd 入口
2. pipe / fork / dup2 / exec 已经能在用户态自己组合

## 6. 验证命令

推荐验证：

```text
run pipeline_demo
run pipe_test
run dup2_test
run fork_fd_test
run pipe_fork_dup2_test
run pipe_close_test
run exec_fd_test
```

其中 `run pipeline_demo` 预期至少能看到：

1. `pipeline_demo: start`
2. `pipeline_reader got:`
3. `pipeline writer line 1`
4. `pipeline writer line 2`
5. `pipeline writer line 3`
6. `pipeline_reader: ok`
7. `pipeline_demo: ok`

## 7. 当前限制

当前仍然不是完整 UNIX pipeline：

1. 只有一个全局教学版 pipe buffer
2. 没有并发阻塞 pipe
3. 没有多个独立 pipe object
4. 没有多级管道
5. 没有完整 POSIX `fork / dup2 / exec / pipe` 语义
6. 没有引用计数
7. 没有 close-on-exec

## 8. 后续方向

Task90 之后，可以继续往这些方向推进：

1. Task91：exec 参数传递整理
2. Task92：阻塞 pipe / 并发 pipe 雏形
3. Task93：多 pipe object 或更接近真实 UNIX pipeline 的生命周期整理
