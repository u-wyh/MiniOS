# Task92：用户态 pipeline demo 支持带参数程序 / argv + pipe 组合验证

## 1. 任务目标

Task92 的目标是在 Task90 和 Task91 的基础上，新增一个更接近真实 shell pipeline 的用户态 demo，验证：

1. `pipe()`
2. `fork()`
3. `dup2()`
4. `exec(argc, argv)`

已经能组合起来工作。

## 2. 为什么做这一步

Task90 只验证了固定程序：

```text
pipeline_writer | pipeline_reader
```

Task91 补齐了 `exec` 的最小 `argc / argv` 语义。

Task92 要验证的是：右侧程序不再只是一个固定 reader，而可以是带参数的现有文本工具，例如：

```text
pipeline_writer | grep MiniOS
```

## 3. 修改文件

本轮主要修改：

1. `include/user_program.h`
2. `include/fs.h`
3. `kernel/fs_parts/embedded_and_tables.inc`
4. `kernel/user_program_sources/pipeline_writer_elf_source.c`
5. `kernel/user_program_sources/pipeline_args_demo_elf_source.c`
6. `kernel/user_program_blobs/pipeline_args_demo_elf.inc`
7. `readme.md`
8. `docs/phase2.md`
9. `docs/fd.md`
10. `docs/process.md`
11. `docs/pipe.md`
12. `docs/user_programs.md`

## 4. 实现思路

当前继续采用教学版顺序模型：

1. `pipeline_args_demo` 先 `pipe(fds)`
2. `fork()` writer 子进程
3. writer 子进程 `dup2(fds[1], 1)` 后 `exec(pipeline_writer)`
4. 父进程 `waitpid(writer)`
5. 再 `fork()` consumer 子进程
6. consumer 子进程 `dup2(fds[0], 0)` 后 `exec(grep, argc=2, argv={"grep","MiniOS"})`
7. 父进程 `waitpid(consumer)`
8. 最终输出 `pipeline_args_demo: ok`

## 5. 核心语义

这条链路同时验证：

1. writer 端 `fd=1` 仍能在 `exec` 后保留为 pipe 写端
2. consumer 端 `fd=0` 仍能在 `exec` 后保留为 pipe 读端
3. consumer 在 `exec` 后还能收到自己的参数 `argv[1] = "MiniOS"`
4. 现有用户态文本工具已经可以作为 pipeline 右侧程序使用

## 6. 验证命令

推荐验证：

```text
run pipeline_args_demo
run pipeline_demo
run exec_args_test
run pipe_test
run dup2_test
run fork_fd_test
run pipe_fork_dup2_test
run pipe_close_test
```

其中 `run pipeline_args_demo` 当前预期至少能看到：

1. `pipeline_args_demo: start`
2. `MiniOS line one`
3. `MiniOS line three`
4. `pipeline_args_demo: ok`

## 7. 当前限制

当前仍然不是完整 UNIX pipeline：

1. 只有一个全局教学版 pipe buffer
2. 没有并发阻塞 pipe
3. 没有多级管道
4. 没有完整 POSIX `execve`
5. 没有动态 argv 解析器

## 8. 后续方向

Task92 之后，可以继续往这些方向推进：

1. Task93：整理 shell argv parser
2. Task94：阻塞 pipe / 并发 pipe 雏形
3. Task95：让用户态 pipeline demo 支持 `head -n 2` / `wc` / `sort`
