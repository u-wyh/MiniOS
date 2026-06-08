# MiniOS Phase2 Pipe 文档

## 1. 当前定位

当前 MiniOS Phase2 的 pipe 不是完整 UNIX pipe，但已经不再是最早期那种“单全局顺序缓冲区”。

最终态下它更准确的定位是：

```text
pipe fd
  -> pipe_id
    -> pipe_table[pipe_id]
      -> 独立 pipe object
```

当前已经具备：

1. 多个独立 pipe object
2. `pipe()` syscall
3. `FD_PIPE_READ / FD_PIPE_WRITE`
4. `fork + dup2 + exec` 组合
5. `mini_pipeline` 多级管道
6. shell 多级 `|` 语法入口

但它仍然是教学版实现：

1. 不是完整 POSIX pipe
2. 没有完整引用计数
3. 阻塞等待仍然是教学版 busy-wait / retry
4. 没有 signal / SIGPIPE / 进程组

## 2. pipe fd 与 pipe buffer

当前教学版 pipe 已经进入 fd 体系。

最小语义是：

1. `pipe read fd`
   只能读，不能写。
2. `pipe write fd`
   只能写，不能读。
3. 两者通过 `pipe_id` 绑定到同一个 pipe object。
4. 不同 pipe fd 可以绑定到不同的 pipe object。

也就是说，这一轮还不是完整 UNIX pipe，只是把“左右端”先抽象进 fd 类型。

Task81 之后，左右端与标准入口的关系又前进了一步：

1. 左侧 `stdout`
   可以通过内核内部 `fd_dup2(pipe_write_fd, 1)` 绑定到 `fd=1`
2. 右侧 `stdin`
   可以通过内核内部 `fd_dup2(pipe_read_fd, 0)` 绑定到 `fd=0`

当前已经有教学版用户态 `dup2()` syscall，但仍不是完整 POSIX `dup2`。

## 3. pipe buffer 结构

当前最终态的核心结构是：

1. `pipe_table[PROCESS_MAX_PIPE_OBJECTS]`
   保存固定数量的 pipe object。
2. 每个 pipe object 至少包含：
   - `used`
   - `active`
   - `overflowed`
   - `read_open`
   - `write_open`
   - `data`
   - `read_pos`
   - `write_pos`
   - `count`
3. `fd_table[]` 里的 pipe fd 通过 `pipe_id` 找到对应对象。

## 4. 容量限制

当前每个 pipe object 的固定容量是：

```text
PROCESS_PIPE_BUFFER_SIZE = 512
```

当前 pipe object 数量上限是：

```text
PROCESS_MAX_PIPE_OBJECTS = 8
```

当前不支持：

1. 动态扩容
2. 环形缓冲区
3. 阻塞等待空间
4. 背压机制

## 5. 写入流程

当进程的 `stdout` 被配置为写 pipe，或用户态直接对 `pipe write fd` 调用 `write(fd, ...)` 时：

1. `SYS_WRITE` 进入内核
2. syscall 分发识别到当前进程启用了 `stdout -> pipe`
3. 当前进程会通过自己绑定的 `pipe write fd` 进入 `process_write_pipe_fd(...)`
4. 根据 `pipe_id` 找到对应 pipe object
5. 写入前先检查剩余空间
6. 写入后更新 `write_pos / count`

当前写满时的行为：

1. 不允许越界写
2. 尽量把剩余空间写满
3. 只输出一次：

```text
pipe: buffer full
```

4. 后续继续写时返回 `0`
5. 不 panic

这是一种教学版“截断 + 单次提示”策略，不追求 POSIX 语义。

## 6. 读取流程

当进程的 `stdin` 被配置为从 pipe 读取，或用户态直接对 `pipe read fd` 调用 `read(fd, ...)` 时：

1. 用户态执行 `SYS_READ(fd=0, ...)`
2. 当前进程会把 `fd=0` 映射到自己绑定的 `pipe read fd`
3. 内核进入 `process_read_pipe_fd(...)`
4. 根据 `pipe_id` 找到对应 pipe object
5. 从 `read_pos` 开始读取
6. 读取成功后推进 `read_pos`，并维护 `count`

EOF 语义：

1. 写端已关闭且 `count == 0` 时返回 `0`
2. 空 pipe 且写端仍开着时不会立刻 EOF，而是等待
3. 非法端点调用不会 panic，而是返回错误

## 7. 初始化与清理

当前最终态的初始化与清理更准确地说是：

1. `pipe()` 创建时，从 `pipe_table[]` 分配一个独立 pipe object
2. 读端和写端 fd 都记录同一个 `pipe_id`
3. `fork()` / `dup2()` 复制 pipe fd 时，本质上复制的是 `pipe_id`
4. `close()` / `exit()` 时，会更新该 `pipe_id` 对应对象的 `read_open / write_open`
5. 当两端都关闭后，对应 pipe object 槽位会被回收复用

shell / `mini_pipeline` 的生命周期管理则是：

1. shell 多级 `|` 会转交给 `mini_pipeline`
2. `mini_pipeline` 根据命令段数量创建 `N-1` 个 pipe
3. 每个子进程通过 `dup2(pipe_fd, 0/1)` 接线
4. 父进程会关闭自己手里的多余 pipe fd，再统一 `waitpid()`

## 8. 与 redirect 的组合

当前支持的典型组合包括：

1. `run A | run B`
2. `run A | run B > output`
3. `run A < input | run B`
4. `run A < input | run B > output`

当前约束下的分工是：

1. 左侧 `stdout` 若指向 pipe，则不再落到屏幕
2. 右侧 `stdin` 若来自 pipe，则从 pipe buffer 读
3. 右侧 `stdout` 若继续重定向，则写 RAMFS 文件
4. 左侧若还有 `stdin redirect`，会先从输入文件读取，再写入 pipe

从最终态来看，可以更清楚地把它理解成：

1. 左侧 `stdout` 通过绑定的 `pipe write fd` 写入 pipe
2. 右侧 `stdin` 通过绑定的 `pipe read fd` 读取 pipe
3. 首段 `< input` 与末段 `> output` / `>> output` 可以继续组合
4. shell `|` 最终会被翻译成 `mini_pipeline ... -- ... -- ...`

## 9. 演进说明

下面 Task79~Task99 的小节保留的是阶段演进记录。

其中早期出现的：

1. “只有一个全局 pipe buffer”
2. “顺序 pipe”
3. “还没有用户态 dup2 / pipe()”

这些说法都只表示对应任务当时的阶段状态，不代表当前 Phase2 最终态。

Task84 之后，MiniOS 额外新增了教学版 `pipe()` syscall 雏形：

1. 当前进程可以显式创建一对 pipe fd
2. `fds[0]` 是读端
3. `fds[1]` 是写端
4. 这对 fd 仍然只绑定到同一个全局教学版 pipe buffer

因此它还不是完整 UNIX pipe object，只是把“shell 内部 pipe”进一步向“用户可见 fd 资源”推进了一步。

Task85 之后，pipe fd 还可以继续通过用户态 `dup2` 复制：

1. `dup2(pipe_write_fd, newfd)`
   - 可以把 pipe 写端复制到另一个教学版 fd 位置
2. `dup2(pipe_read_fd, newfd)`
   - 可以把 pipe 读端复制到另一个教学版 fd 位置
3. 这为后续用户态自己组合 `pipe + dup2 + fork` 奠定了接口基础
4. 这一步当时仍然没有 fork 后共享 pipe fd，也没有多个独立 pipe object

Task86 之后，fork 也会继承当前教学版 pipe fd 视图：

1. 父进程先创建 `pipe(fds)`
2. `fork()` 后子进程会复制这对 pipe fd 的表项和绑定字段
3. 子进程可以直接使用继承下来的 pipe write fd 写入
4. 父进程在 `waitpid()` 后仍可从原 pipe read fd 读出数据
5. 这里描述的是 Task86 当时的阶段状态；最终态已在 Task97 之后推进到多个独立 pipe object

Task87 之后，用户态已经可以把这条链路自己组合出来：

1. `pipe(fds)`
2. `fork()`
3. 子进程 `dup2(fds[1], 1)`
4. 子进程通过 `write(1, ...)` 写 pipe
5. 父进程 `dup2(fds[0], 0)`
6. 父进程通过 `read(0, ...)` 读 pipe

这说明当前 MiniOS 已经具备最小“用户态自己搭管道”的实验条件，但仍然不是完整 UNIX pipeline。

Task88 之后，教学版 pipe close 语义也更清楚了：

1. `close(pipe read fd)`：
   - 标记读端关闭
   - 不主动丢弃 buffer 中已有数据
2. `close(pipe write fd)`：
   - 标记写端关闭
   - 已写入的数据仍可继续被读端消费
3. 写端关闭后的 read：
   - 若 buffer 里还有数据，继续正常读取
   - 读完后返回 EOF
4. 读端关闭后的 write：
   - 返回错误或 0
   - 不 panic
5. 当前仍不支持：
   - 阻塞 read/write
   - SIGPIPE
   - EPIPE errno
   - 引用计数
   - 多个并发 pipe object

因此这仍然只是教学版 close 语义整理，不是完整 UNIX pipe 生命周期。

Task89 之后，pipe / fork / dup2 / exec 这条路线已经能串起来：

1. `pipe(fds)`
2. `fork()`
3. 子进程 `dup2(fds[1], 1)`
4. 子进程 `exec(exec_fd_writer)`
5. `exec` 后 writer 程序继续通过 `write(1, ...)` 写 pipe
6. 父进程从 `fds[0]` 读回数据

这说明当前教学版系统已经具备最小“exec 后保留 pipe stdout 绑定”的条件，
但仍然不是完整 UNIX pipeline，也还没有并发阻塞 pipe。

Task90 之后，这条路线已经进一步扩展成一个完整的用户态 demo：

1. `pipeline_demo`
   - 自己调用 `pipe(fds)`
   - 自己 `fork()` writer 子进程
   - writer 子进程 `dup2(fds[1], 1)` 后 `exec(pipeline_writer)`
   - 父进程等待 writer 结束
   - 父进程再 `fork()` reader 子进程
   - reader 子进程 `dup2(fds[0], 0)` 后 `exec(pipeline_reader)`
2. `pipeline_writer`
   - 只通过 `write(1, ...)` 输出固定文本
3. `pipeline_reader`
   - 只通过 `read(0, ...)` 读取，再通过 `write(1, ...)` 输出

这说明当前教学版 pipe 已经不仅能被 shell 内部使用，也能被用户态程序自己用 `pipe + fork + dup2 + exec` 组合出最小 `producer | consumer` 演示。

Task92 之后，这个教学版 pipeline demo 还可以继续承接带参数 consumer：

1. writer 继续是 `pipeline_writer`
2. consumer 切换成 `grep MiniOS`
3. `grep` 的关键字参数通过 `exec(argc, argv)` 传入
4. `grep` 再从 `fd=0` 读取 pipe 数据并输出匹配结果

这说明当前教学版 pipe 已经能和：

1. `dup2`
2. `exec`
3. `argv`

一起工作，而不只是跑固定 writer/reader 程序。

Task94 之后，这条教学版 pipe 路线又进一步收敛成一个更像命令的用户态入口：

1. `run mini_pipeline <left_prog> [left_args...] -- <right_prog> [right_args...]`
2. `mini_pipeline` 自己调用：
   - `pipe(fds)`
   - `fork()`
   - `dup2()`
   - `exec(argc, argv)`
3. 左侧现在也支持带参数程序：
   - `cat /readme.txt`
   - `pipeline_writer`
4. 右侧支持带参数程序：
   - `grep MiniOS`
   - `head -n 3`
   - `wc`
5. 当前仍然采用教学版顺序模型：
   - 左右两侧都先建立
   - 左侧边写 pipe，右侧边从同一全局 pipe buffer 读
   - 父进程最后分别等待它们退出

这说明当前教学版 pipe 已经不只是 shell 内部机制、测试 syscall 或固定 demo，而是开始具备一个最小可复用的用户态 pipeline 命令入口。

## 9. Task96：教学版 busy-wait pipe

Task96 之后，当前 pipe 的最小等待语义变成：

1. 读端：
   - 若缓冲区里已有数据，直接读
   - 若缓冲区为空且写端还开着，则做最小 busy-wait
   - 若写端已关闭且数据已读完，则返回 EOF
2. 写端：
   - 若缓冲区还有空间，继续写
   - 若缓冲区满且读端还开着，则做最小 busy-wait
   - 若读端已关闭，则返回错误
3. 当前仍只有一个全局 pipe buffer：
   - 没有多个 pipe object
   - 没有完整 sleep/wakeup 队列
   - 没有 SIGPIPE / EPIPE

为了支持“大于 pipe buffer 的文本流”，当前实现会在读端消费数据后，把未读部分压回缓冲区开头，再继续让写端推进新数据。

## 10. 当前不支持的真实 UNIX pipe 能力

当前教学版 pipe 还不支持：

1. fork 后共享 pipe fd
2. 两端并发执行
3. 阻塞读写
4. 多级管道
5. 动态扩容
6. 多个 pipe object
7. 调度器驱动的生产者/消费者并发协作
8. 完整 POSIX `dup2` / `pipe` 生命周期语义

所以它更像“内核里的一个临时顺序缓冲区”，而不是完整 UNIX pipe 子系统。

## 11. Task97：pipe_table 与多个 pipe object

Task97 之后，教学版 pipe 的核心结构已经从“单全局 pipe buffer”推进到：

```text
pipe_table[PROCESS_MAX_PIPE_OBJECTS]
```

每个 pipe object 现在独立维护：

1. `used`
2. `active`
3. `data[PROCESS_PIPE_BUFFER_SIZE]`
4. `read_pos`
5. `write_pos`
6. `count`
7. `read_open`
8. `write_open`

这意味着：

1. `pipe()` 每次会分配一个空闲 `pipe_id`
2. `fds[0]` 绑定为 `FD_PIPE_READ + pipe_id`
3. `fds[1]` 绑定为 `FD_PIPE_WRITE + pipe_id`
4. `read/write/close` 都先经由 fd 表项找到 `pipe_id`
5. 不同 pipe object 的数据不会互相污染

当前最小分配/回收语义是：

1. `pipe_alloc()`
   - 扫描 `pipe_table[]`
   - 找一个 `used == 0` 的对象
   - 初始化 buffer / pos / count / open 状态
2. `pipe_try_free(pipe_id)`
   - 当 `read_open == 0 && write_open == 0` 时清空对象
   - 对象槽位可以被后续新的 `pipe()` 复用

当前仍然不是完整 POSIX 生命周期：

1. 没有复杂引用计数
2. fork/dup2 只是复制 `pipe_id`
3. close 语义仍是教学版近似模型
4. 还不支持多级 Shell pipeline

## 12. Task98：多级 mini_pipeline 与多个 pipe 串联

Task98 之后，`mini_pipeline` 已经可以把多个命令段串起来：

```text
run mini_pipeline <cmd1> [args...] -- <cmd2> [args...] -- <cmd3> [args...] ...
```

这里的关键点是：

1. 若有 `N` 个命令段，就需要 `N-1` 个 pipe
2. 每个 pipe 都必须是独立 pipe object
3. 相邻命令段通过各自对应的 pipe object 串联

例如：

```text
run mini_pipeline cat /readme.txt -- grep MiniOS -- wc
```

需要：

1. `pipe0`
   - `cat stdout -> grep stdin`
2. `pipe1`
   - `grep stdout -> wc stdin`

因此多级 pipeline 中每个子进程的接线关系是：

1. 第一个命令：
   - `fd=1 -> pipe0 write`
2. 中间命令：
   - `fd=0 -> pipe(i-1) read`
   - `fd=1 -> pipe(i) write`
3. 最后一个命令：
   - `fd=0 -> 最后一个 pipe read`

这也是 Task97 多个 pipe object 的直接组合验证：

1. `pipe0` 和 `pipe1` 不能串数据
2. 父进程和子进程都必须及时关闭自己不需要的 pipe fd
3. 否则下游 reader 可能永远观察不到 EOF

## 13. Task99：shell 原生多级 `|` 接入 mini_pipeline

Task99 没有重写 pipe object，也没有重写 `mini_pipeline` 的底层数据流；它做的是把 shell 原生：

```text
run A ... | run B ... | run C ...
```

翻译成一次等价的：

```text
run mini_pipeline A ... -- B ... -- C ...
```

因此当前 shell 多级 pipeline 的 pipe 语义，本质上仍然继承自 Task98：

1. 若有 `N` 段命令，则创建 `N-1` 个 pipe object
2. 相邻两段命令之间各自用独立 pipe object 连接
3. 第一个命令只接 stdout
4. 中间命令同时接 stdin 和 stdout
5. 最后一个命令只接 stdin

当前额外的 shell 侧限制是：

1. 每段都必须显式写 `run`
2. 只允许首段 `< input`
3. 只允许末段 `> output` 或 `>> output`
4. 中间段若出现 `<` / `>` / `>>` 会直接报错

## 结尾说明

如果想先看 Phase2 总图，再回来对照 pipe 细节，建议配合：

1. [phase2_summary.md](/home/wyh/MiniOS/phase2_kernel_os/docs/phase2_summary.md)
2. [fd.md](/home/wyh/MiniOS/phase2_kernel_os/docs/fd.md)
3. [process.md](/home/wyh/MiniOS/phase2_kernel_os/docs/process.md)
4. [phase3_plan.md](/home/wyh/MiniOS/phase2_kernel_os/docs/phase3_plan.md)
