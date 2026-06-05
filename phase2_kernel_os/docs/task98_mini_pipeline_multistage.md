# Task98：mini_pipeline 支持多级管道

## 1. 任务目标

把 `mini_pipeline` 从二段命令：

```text
run mini_pipeline <left_prog> [left_args...] -- <right_prog> [right_args...]
```

推进成多级格式：

```text
run mini_pipeline <cmd1> [args...] -- <cmd2> [args...] -- <cmd3> [args...] ...
```

重点验证多个 pipe object 是否真的能把多个用户程序串成一条多级 pipeline。

## 2. 为什么需要多级 pipeline

二段 pipeline 只能验证：

1. 一个 writer
2. 一个 reader
3. 一条 pipe

但真实的数据流更常见的是：

1. `cat /readme.txt | grep MiniOS | wc`
2. `cat /readme.txt | head -n 5 | tail -n 2`

这类链路要求：

1. `N` 个命令
2. `N-1` 个 pipe
3. 中间命令同时连接 stdin 和 stdout

因此 Task98 的意义就是把 Task97 的多个 pipe object 真正“用起来”。

## 3. 修改文件

本轮主要修改：

1. `kernel/user_program_sources/mini_pipeline_elf_source.c`
2. `kernel/user_program_blobs/mini_pipeline_elf.inc`
3. `README.md`
4. `docs/phase2.md`
5. `docs/user_programs.md`
6. `docs/pipe.md`
7. `docs/process.md`
8. `docs/fd.md`

## 4. 实现思路

### 4.1 多段命令解析

当前 `mini_pipeline` 会把自己的 `argv` 用 `--` 拆成多个命令段。

例如：

```text
run mini_pipeline cat /readme.txt -- grep MiniOS -- wc
```

会被拆成：

1. `["cat", "/readme.txt"]`
2. `["grep", "MiniOS"]`
3. `["wc"]`

`--` 本身不会进入任何命令段的 `argv`。

### 4.2 N 个命令对应 N-1 个 pipe

若命令段数量为 `N`，则：

```text
pipe_count = N - 1
```

例如三段命令需要：

1. `pipe0`
2. `pipe1`

### 4.3 fd 接线

每个命令段根据位置决定：

1. 第一个命令：
   - 只把 `fd=1` 接到第一个 pipe write
2. 中间命令：
   - `fd=0` 接前一个 pipe read
   - `fd=1` 接后一个 pipe write
3. 最后一个命令：
   - 只把 `fd=0` 接到最后一个 pipe read

### 4.4 close 策略

为保证 EOF 能正确到达：

1. 子进程在 `dup2` 之后关闭自己手里的所有原始 pipe fd
2. 父进程在 fork 完所有子进程后关闭自己手里的所有 pipe fd

## 5. 命令格式

当前支持：

```text
run mini_pipeline cat /readme.txt -- grep MiniOS -- wc
run mini_pipeline cat /readme.txt -- head -n 5 -- tail -n 2
run mini_pipeline pipeline_writer -- grep MiniOS -- wc
```

同时继续兼容旧的二段格式。

## 6. fd / pipe 数据流

以：

```text
run mini_pipeline cat /readme.txt -- grep MiniOS -- wc
```

为例：

1. `cat`
   - `fd=1 -> pipe0 write`
2. `grep`
   - `fd=0 -> pipe0 read`
   - `fd=1 -> pipe1 write`
3. `wc`
   - `fd=0 -> pipe1 read`

这正是多个 pipe object 串联多个进程的最小模型。

## 7. 验证命令

本轮重点验证：

```text
run mini_pipeline cat /readme.txt -- grep MiniOS -- wc
run mini_pipeline cat /readme.txt -- head -n 5 -- tail -n 2
run mini_pipeline pipeline_writer -- grep MiniOS -- wc
run mini_pipeline cat /readme.txt -- grep MiniOS
run mini_pipeline cat /readme.txt -- head -n 3
run mini_pipeline pipeline_writer -- wc
```

## 8. 当前限制

当前仍然不支持：

1. 真正 shell 的 `|`
2. 引号和转义
3. 环境变量
4. 进程组
5. 完整 POSIX pipe 生命周期
6. 完整 fd 引用计数

## 9. 后续方向

下一步可以继续推进：

1. Task99：真正 Shell 多级管道解析雏形
2. Task100：Phase2 fd / pipe / process 总结文档
3. Task101：测试清单和阶段收尾
