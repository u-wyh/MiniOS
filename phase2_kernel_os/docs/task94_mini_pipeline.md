# Task94：mini_pipeline 命令 / 用户态固定管道命令入口

## 1. 任务目标

本轮新增一个用户态程序：

- `mini_pipeline`

它不是完整 shell pipeline，而是一个固定格式的教学版 pipeline 入口：

```text
run mini_pipeline <left_prog> -- <right_prog> [right_args...]
```

## 2. 为什么现在做这件事

前面已经分别完成了：

- `pipe()`
- `dup2()`
- `fork` 后 fd 继承
- `exec` 后 fd 保留
- `exec argc/argv`
- `pipeline_demo`
- `pipeline_args_demo`

Task94 的意义是把这些分散验证过的机制收敛成一个更像命令的入口，而不是继续只停留在测试程序层面。

## 3. 修改文件

- `include/user_program.h`
- `include/fs.h`
- `kernel/fs_parts/embedded_and_tables.inc`
- `kernel/user_program_sources/mini_pipeline_elf_source.c`
- `kernel/user_program_blobs/mini_pipeline_elf.inc`
- `readme.md`
- `docs/phase2.md`
- `docs/user_programs.md`
- `docs/pipe.md`
- `docs/process.md`
- `docs/fd.md`

## 4. 实现思路

当前 `mini_pipeline` 采用最小顺序模型：

1. 解析自己的 `argv`
2. 检查 `--` 是否存在
3. 左侧只接受一个程序名
4. 右侧接受程序名加普通参数
5. `pipe(fds)`
6. `fork()` 左侧 writer 子进程
7. 左侧子进程 `dup2(fds[1], 1)` 后 `exec(left_prog)`
8. 父进程 `waitpid(writer)`
9. `fork()` 右侧 consumer 子进程
10. 右侧子进程 `dup2(fds[0], 0)` 后 `exec(right_prog, argv)`
11. 父进程 `waitpid(consumer)`
12. 输出 `mini_pipeline: ok`

## 5. 核心语义

### 参数格式

例如：

```text
run mini_pipeline pipeline_writer -- grep MiniOS
```

会解析为：

- left_prog：`pipeline_writer`
- right_prog：`grep`
- right_argv：`["grep", "MiniOS"]`

再例如：

```text
run mini_pipeline pipeline_writer -- head -n 2
```

会解析为：

- left_prog：`pipeline_writer`
- right_prog：`head`
- right_argv：`["head", "-n", "2"]`

### 当前限制

- 左侧暂不支持参数
- 右侧支持普通参数
- 仍然只有一个全局教学版 pipe buffer
- 仍然采用“先左侧写完，再右侧读取”的顺序模型

## 6. 验证命令

建议重点验证：

```text
run mini_pipeline pipeline_writer -- grep MiniOS
run mini_pipeline pipeline_writer -- head -n 2
run mini_pipeline pipeline_writer -- wc
run mini_pipeline
run mini_pipeline pipeline_writer
run mini_pipeline -- grep MiniOS
run mini_pipeline pipeline_writer --
```

## 7. 当前限制

- 不是完整 shell pipeline
- 不支持多级管道
- 不支持左侧参数
- 不支持并发阻塞 pipe
- 不支持多个独立 pipe object

## 8. 后续方向

- 后续可以在这个基础上继续做更像真实 shell 的 `mini_pipeline` 扩展
- 也可以继续推进阻塞 pipe、并发 pipe、多级管道
