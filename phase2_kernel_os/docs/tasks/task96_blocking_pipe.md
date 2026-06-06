# Task96：阻塞 pipe / 并发 pipeline 雏形

## 1. 任务目标

把当前教学版 pipe 从“左侧先写完、右侧再读取”的顺序模型，推进到最小并发模型：

1. 左侧和右侧都先 `fork` 出来
2. 左侧写 pipe
3. 右侧读 pipe
4. pipe 空/满时不直接失败，而是做教学版等待

## 2. 为什么做这个任务

Task95 之后，`mini_pipeline` 已经支持双端 `argv`，但如果 pipe 仍然是顺序模型：

1. 左侧必须先完整结束
2. 右侧才开始读取

那它仍然不够像真实 UNIX pipeline，也无法自然支撑超过固定缓冲区的小流式传输。

## 3. 修改文件

1. `include/process.h`
2. `kernel/process.c`
3. `kernel/process_parts/core_helpers.inc`
4. `kernel/process_parts/fd_and_input.inc`
5. `kernel/process_parts/redirect_pipe.inc`
6. `kernel/user_program_sources/mini_pipeline_elf_source.c`
7. `readme.md`
8. `docs/phase2.md`
9. `docs/user_programs.md`
10. `docs/pipe.md`
11. `docs/process.md`
12. `docs/fd.md`

## 4. 实现思路

1. 保留单全局教学版 pipe buffer。
2. 读端消费数据后，把未读部分压回缓冲区开头。
3. 写端写满时不直接丢弃，而是等待 reader 继续消费。
4. 读端读空时，如果写端还开着，则等待 writer 继续写。
5. `mini_pipeline` 先建立左右两个子进程，父进程最后再 wait。

## 5. 核心语义

当前 pipe 语义：

1. 空 pipe 且写端仍打开：read busy-wait。
2. 写端关闭且数据读完：read 返回 0，表示 EOF。
3. 满 pipe 且读端仍打开：write busy-wait。
4. 读端关闭：write 返回错误，不 panic。

当前 `mini_pipeline` 语义：

1. 左右子进程并发存在。
2. 父进程关闭自己的 pipe 端点，避免错误保持写端/读端引用。
3. 父进程分别 `waitpid(writer)` 与 `waitpid(reader)`。

## 6. 当前限制

1. 仍然只有一个全局 pipe buffer。
2. 不是完整 POSIX pipe。
3. 没有 sleep/wakeup 队列。
4. 当前等待机制是 busy-wait。
5. 不支持多级管道。
6. 不支持多个独立 pipe object。

## 7. 验证命令

建议验证：

```text
run mini_pipeline cat /readme.txt -- grep MiniOS
run mini_pipeline cat /readme.txt -- wc
run mini_pipeline cat /readme.txt -- head -n 3
run mini_pipeline cat /readme.txt -- sort
run mini_pipeline pipeline_writer -- grep MiniOS
run mini_pipeline pipeline_writer -- head -n 2
run mini_pipeline pipeline_writer -- wc
```

以及回归：

```text
run pipe_close_test
run pipe_test
run dup2_test
run fork_fd_test
run pipe_fork_dup2_test
run pipeline_demo
run pipeline_args_demo
```

## 8. 后续方向

1. 引入真正 sleep/wakeup 的 pipe 等待队列。
2. 引入多个独立 pipe object。
3. 进一步靠近真实 UNIX pipeline 和完整 shell `|` 语法。
