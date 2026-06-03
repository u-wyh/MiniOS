# MiniOS Phase2 Pipe 文档

## 1. 当前定位

当前 MiniOS Phase2 的 pipe 不是完整 UNIX pipe。

它是一个教学版顺序 pipe，目标是验证：

```text
左侧程序 stdout
  -> 内核 pipe buffer
    -> 右侧程序 stdin
```

也就是说，当前并不是两个进程并发读写同一个 pipe，而是：

1. shell 先运行左侧程序
2. 左侧程序把输出完整写到 pipe buffer
3. 左侧程序结束
4. shell 再运行右侧程序
5. 右侧程序从 pipe buffer 读取直到 EOF

## 2. pipe fd 与 pipe buffer

Task79 之后，当前教学版 pipe 已经开始进入 fd 体系。

最小语义是：

1. `pipe read fd`
   只能读，不能写。
2. `pipe write fd`
   只能写，不能读。
3. 两者都不直接持有独立 pipe object。
4. 当前仍然统一绑定到同一个全局教学版 pipe buffer。

也就是说，这一轮还不是完整 UNIX pipe，只是把“左右端”先抽象进 fd 类型。

Task81 之后，左右端与标准入口的关系又前进了一步：

1. 左侧 `stdout`
   可以通过内核内部 `fd_dup2(pipe_write_fd, 1)` 绑定到 `fd=1`
2. 右侧 `stdin`
   可以通过内核内部 `fd_dup2(pipe_read_fd, 0)` 绑定到 `fd=0`

当前仍然没有用户态 `dup2()` syscall，这只是内核内部统一入口雏形。

## 3. pipe buffer 结构

当前 pipe buffer 位于进程子系统里，核心状态包括：

1. `data`
   保存当前 pipe 的字节内容。
2. `size`
   表示当前 buffer 中已有多少有效字节。
3. `read_offset`
   表示右侧程序已经读取到哪个位置。
4. `active`
   表示当前是否正处在一条教学版 pipe 命令的生命周期内。
5. `overflowed`
   表示本次 pipe 是否已经发生过“写满”事件，用来保证错误提示只输出一次。

## 4. 容量限制

当前 pipe buffer 的固定容量是：

```text
PROCESS_PIPE_BUFFER_SIZE = 512
```

当前不支持：

1. 动态扩容
2. 环形缓冲区
3. 阻塞等待空间
4. 背压机制

## 5. 写入流程

当左侧程序的 `stdout` 被配置为写 pipe 时：

1. `SYS_WRITE` 进入内核
2. syscall 分发识别到当前进程启用了 `stdout -> pipe`
3. 当前进程会通过自己绑定的 `pipe write fd` 进入 `process_write_pipe_fd(...)`
4. 写入前先计算剩余空间
5. 只把还能放下的字节写进 `data`
6. 更新 `size`

如果用户态或内核路径直接调用 `SYS_FD_WRITE(fd, ...)`：

1. 普通文件 fd 仍走文件写入逻辑
2. `pipe write fd` 会写入同一个教学版 pipe buffer
3. `pipe read fd` 上调用写会返回错误

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

当右侧程序的 `stdin` 被配置为从 pipe 读取时：

1. 用户态执行 `SYS_READ(fd=0, ...)`
2. 当前进程会把 `fd=0` 映射到自己绑定的 `pipe read fd`
3. 内核进入 `process_read_pipe_fd(...)`
4. 从 `data + read_offset` 开始拷贝
5. 最多读取剩余的 `size - read_offset` 字节
6. 读取成功后推进 `read_offset`

如果用户态或内核路径直接调用 `SYS_READ(fd, ...)`：

1. 普通文件 fd 仍走文件读取逻辑
2. `pipe read fd` 从 pipe buffer 读取
3. `pipe write fd` 上调用读会返回错误

EOF 语义：

1. `read_offset >= size` 时返回 `0`
2. 空 pipe 返回 `0`
3. 未激活 pipe 的路径下也不会 panic

因此当前 pipe 的读完语义就是最小 EOF 语义。

## 7. 初始化与清理

每次执行 `run A | run B` 时：

1. shell 在执行前调用 `pipe reset`
2. 清空 `active / size / read_offset / overflowed / data 语义状态`
3. 运行左侧程序并写入 pipe
4. 运行右侧程序并读取 pipe
5. 执行结束后再次 `pipe reset`

这样做的目的是：

1. 避免连续 pipe 命令复用旧数据
2. 避免右侧读到上一次命令残留
3. 让每条 pipe 命令拥有独立的最小生命周期

Task80 之后，pipe 的 fd 生命周期也更清楚了一些：

1. 左侧子进程会绑定一个 `pipe write fd`
2. 右侧子进程会绑定一个 `pipe read fd`
3. `close`、进程清空和 pipe reset 现在会复用更统一的 fd 槽位重置逻辑
4. 兼容字段仍保留，但 pipe 读写分发已经优先走 fd 类型

Task81 之后，pipe 配置路径进一步统一为：

1. 先分配 `pipe read/write fd`
2. 再通过内核内部 `fd_dup2` 把它们接到 `fd=0 / fd=1`
3. 仍然保留兼容字段，避免影响已有 shell 行为

Task82 与 Task83 串起来后的状态是：

1. `<` 与 `>` 文件重定向已经开始迁移到 `fd_dup2`
2. `pipe` 连接也已经统一到：
   - 左侧 `fd_dup2(pipe_write_fd, 1)`
   - 右侧 `fd_dup2(pipe_read_fd, 0)`
3. `pipe + redirect` 组合继续走同一套 shell 启动顺序，但仍保留教学版兼容字段

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

从 Task80 开始，可以更清楚地把它理解成：

1. 左侧 `stdout` 通过绑定的 `pipe write fd` 写入 pipe
2. 右侧 `stdin` 通过绑定的 `pipe read fd` 读取 pipe
3. `stdin/stdout` 文件重定向仍然保留原有兼容路径
4. 也就是说，当前是“fd 分发开始统一，但 shell 入口仍保留教学版特殊配置”的过渡阶段

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
4. 当前仍然没有 fork 后共享 pipe fd，也没有多个独立 pipe object

Task86 之后，fork 也会继承当前教学版 pipe fd 视图：

1. 父进程先创建 `pipe(fds)`
2. `fork()` 后子进程会复制这对 pipe fd 的表项和绑定字段
3. 子进程可以直接使用继承下来的 pipe write fd 写入
4. 父进程在 `waitpid()` 后仍可从原 pipe read fd 读出数据
5. 当前仍然只有一个全局 pipe buffer，这只是教学版继承语义，不是完整 UNIX pipe 生命周期模型

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

## 9. 当前不支持的真实 UNIX pipe 能力

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
