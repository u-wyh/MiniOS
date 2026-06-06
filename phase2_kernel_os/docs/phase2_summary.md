# MiniOS Phase2 总结：fd / pipe / process / shell 数据流系统

## 1. Phase2 当前能力总览

当前 Phase2 已经具备这条比较完整的主线：

1. 用户态程序
2. RAMFS
3. fd table
4. open/read/write/close
5. stdin/stdout
6. redirect
7. pipe
8. pipe object table
9. fork
10. dup2
11. exec
12. argv
13. mini_pipeline
14. Shell 多级管道
15. `cat / wc / grep / head / tail / sort`

可以把它概括成：

```text
用户输入 Shell 命令
  -> Shell 解析 argv / redirect / pipe
    -> 创建 fd / pipe
      -> fork 子进程
        -> dup2 绑定 stdin/stdout
          -> exec 目标程序
            -> 用户程序 read(0) / write(1)
```

## 2. fd table 是什么

当前每个进程都有自己的 fd table。

它的作用是：

1. 把用户程序看到的整数 fd 映射到内核里的资源表项
2. 保存这个 fd 是普通文件、pipe read 还是 pipe write
3. 让 `sys_read / sys_write / sys_close` 可以按类型分发

当前教学版 fd 类型至少包括：

1. 普通文件 fd
2. `FD_PIPE_READ`
3. `FD_PIPE_WRITE`

因此要区分两个概念：

1. `fd`
   - 是用户程序手里的整数句柄
2. `fd 表项`
   - 是内核里保存类型、路径、offset、pipe_id 等状态的记录

## 3. stdin / stdout / stderr 当前语义

当前最重要的是：

1. `fd=0` 表示 stdin
2. `fd=1` 表示 stdout
3. `fd=2` 在概念上保留为 stderr，但当前没有完整独立实现

用户程序只需要：

1. `read(0, ...)`
2. `write(1, ...)`

至于 `0/1` 背后到底接的是：

1. 普通文件
2. pipe
3. 默认控制台

由内核通过 redirect / dup2 / pipe 接线来决定。

## 4. open / read / write / close 的分发路径

当前路径可以概括成：

1. `open(path)`
   - 返回一个普通文件 fd
2. `read(fd, ...)`
   - 若是文件 fd，则按文件路径与 offset 读取
   - 若是 `FD_PIPE_READ`，则按 `pipe_id` 从 pipe object 读取
3. `write(fd, ...)`
   - 若是文件 fd，则写 RAMFS
   - 若是 `FD_PIPE_WRITE`，则按 `pipe_id` 写 pipe object
4. `close(fd)`
   - 清理对应 fd 表项
   - 如果是 pipe fd，还要更新对应 pipe object 的 open 状态

也就是说：

1. `argv` 决定“程序要做什么”
2. fd 决定“程序从哪里读、往哪里写”

## 5. redirect 是怎么改变 stdin / stdout 的

当前 redirect 的本质是：

1. `< file`
   - 先打开输入文件得到 fd
   - 再做 `dup2(input_fd, 0)`
2. `> file`
   - 先创建或打开输出文件得到 fd
   - 再做 `dup2(output_fd, 1)`
3. `>> file`
   - 也是先拿到输出文件 fd，再把它接到 `fd=1`

因此 redirect 不是把参数塞给程序，而是改变 `fd=0 / fd=1` 指向的资源。

用户程序并不知道自己被重定向了，它只是继续：

1. `read(0, ...)`
2. `write(1, ...)`

## 6. pipe 的核心模型

当前教学版 `pipe()` 返回两个 fd：

1. `fds[0]`
   - read end
2. `fds[1]`
   - write end

fd 表项里不直接保存整份 pipe 数据，而是保存：

1. `type = FD_PIPE_READ / FD_PIPE_WRITE`
2. `pipe_id`

真正的数据和状态在：

```text
pipe_table[pipe_id]
```

每个 pipe object 当前维护的核心状态包括：

1. buffer
2. `read_pos / write_pos / count`
3. `read_open / write_open`
4. active / overflow 等教学版状态

所以当前关系是：

```text
fd table -> pipe_id -> pipe_table[pipe_id]
```

## 7. pipe object table 的作用

Task97 之后，系统已经不是“所有 pipe 共用一个全局缓冲区”的模型，而是：

1. `pipe_table[PROCESS_MAX_PIPE_OBJECTS]`
2. 每次 `pipe()` 分配一个独立 pipe object
3. 多个 pipe object 之间数据隔离
4. 当读端和写端都关闭后，对象槽位可以回收复用

这也是多级 pipeline 的基础，因为：

1. 若有 `N` 个命令段
2. 就需要 `N-1` 个独立 pipe
3. 中间命令段要同时连接前后两个 pipe

## 8. fork / dup2 / exec 的关系

这三者是当前数据流最关键的一组组合：

1. `fork`
   - 复制当前进程
   - 子进程继承 fd table
2. `dup2`
   - 改变某个 fd 入口指向的资源
   - 重点是把文件或 pipe 接到 `fd=0 / fd=1`
3. `exec`
   - 替换当前进程的程序镜像
   - 但保留 fd table

这三者组合起来，才能支撑 pipeline。

最小示意是：

```text
pipe(fds)
fork()
子进程 dup2(fds[1], 1)
子进程 exec(left)
另一个子进程 / 父进程 dup2(fds[0], 0)
exec(right)
```

## 9. dup2 的本质

当前教学版 `dup2` 的本质是：

1. 让 `newfd` 改为指向 `oldfd` 那个资源
2. 如果 `newfd` 原来已经绑定别的资源，则旧绑定会被覆盖

所以：

1. `dup2(file_fd, 0)` 可以实现输入重定向
2. `dup2(file_fd, 1)` 可以实现输出重定向
3. `dup2(pipe_read_fd, 0)` 可以把 stdin 接到 pipe
4. `dup2(pipe_write_fd, 1)` 可以把 stdout 接到 pipe

当前它已经能产生接近真实 Unix 的使用效果，但还不是完整 POSIX 语义：

1. 引用计数仍然简化
2. 不是完整共享 open file object
3. close-on-exec 等语义还没有

## 10. exec 为什么要保留 fd table

如果 `exec` 会把 fd table 清掉，那么：

1. `dup2(pipe_write_fd, 1)` 先做好的 stdout 接线
2. 在 `exec(writer)` 之后就会丢失

那 pipeline 就没法工作。

所以当前教学版 `exec` 的关键语义是：

1. 替换程序
2. 保留 fd table
3. 新程序继续使用已经配置好的 `fd=0 / fd=1`

这也是：

1. `exec_fd_test`
2. `pipeline_demo`
3. `mini_pipeline`

这些链路能成立的前提。

## 11. argc / argv 和 fd 数据流的区别

这里很容易混淆，必须明确区分：

1. `argv`
   - 是控制参数
   - 决定程序“做什么”
2. fd / stdin / stdout
   - 是数据通道
   - 决定程序“从哪里读、往哪里写”

例如：

1. `grep MiniOS`
   - `MiniOS` 来自 `argv[1]`
2. `grep` 处理的整段输入文本
   - 来自 `fd=0`

所以：

1. `argv` 不是数据流
2. stdin/stdout 才是数据流

## 12. mini_pipeline 是什么

`mini_pipeline` 是当前教学版用户态 pipeline 入口。

它的作用是：

1. 在用户态自己调用 `pipe()`
2. 自己 `fork()` 多个子进程
3. 自己对每段命令做 `dup2(fd=0/1)`
4. 再 `exec(argc, argv)`

Task98 之后它已经支持：

```text
run mini_pipeline <cmd1> [args...] -- <cmd2> [args...] -- <cmd3> [args...] ...
```

也就是：

1. `N` 个命令段
2. 创建 `N-1` 个 pipe
3. 中间命令同时连接 stdin 和 stdout

## 13. Shell 多级管道是怎么工作的

Task99 之后，shell 原生支持：

```text
run cat /readme.txt | run grep MiniOS | run wc
```

但当前 shell 并不是自己重写一套 pipeline 执行器，而是：

1. shell 先把整行切成多个命令段
2. shell 保留每一段自己的 `argv`
3. shell 剥离首段 `< input` 与末段 `> output`
4. shell 把 `|` 翻译成 `mini_pipeline` 使用的 `--`
5. 最终还是执行一次 `mini_pipeline`

因此当前多级 shell 管道的执行关系是：

1. Shell 负责“解析和翻译”
2. `mini_pipeline` 负责“真正建 pipe、fork、dup2、exec、wait”

## 14. 当前与 Linux / UNIX 的差距

当前必须如实承认这些限制：

1. fd 引用计数仍然简化
2. pipe close 不是完整 POSIX 语义
3. 阻塞 pipe 仍然是教学版 busy-wait / sleep-yield-retry
4. 不支持 signal / SIGPIPE
5. 不支持 job control
6. 不支持进程组
7. 不支持完整 tty 抽象
8. 不支持环境变量
9. 不支持完整 shell parser
10. 不支持真实磁盘文件系统
11. 不支持完整 ELF 动态加载
12. 仍然不是完整 POSIX `dup2 / execve / pipe`

所以 Phase2 的定位是：

1. 教学版数据流系统
2. 可以解释清楚 Unix 思想
3. 但还不能夸大成完整 Linux/UNIX 实现

## 15. Phase2 复习重点

复习时建议优先回答这些问题：

1. `dup2` 为什么能实现重定向？
2. `exec` 为什么不能重置 fd table？
3. pipe 为什么必须区分 read end / write end？
4. `fork` 后为什么要继承 fd？
5. Shell pipeline 为什么需要 `N-1` 个 pipe？
6. `argv` 和 stdin 有什么区别？
7. shell 和 `mini_pipeline` 在当前系统里如何分工？
8. 当前 MiniOS 和真实 Linux/UNIX 还差哪几层？

## 16. 交叉阅读

可以继续配合这些专题文档一起看：

1. [fd.md](/home/wyh/MiniOS/phase2_kernel_os/docs/fd.md)
2. [pipe.md](/home/wyh/MiniOS/phase2_kernel_os/docs/pipe.md)
3. [process.md](/home/wyh/MiniOS/phase2_kernel_os/docs/process.md)
4. [shell.md](/home/wyh/MiniOS/phase2_kernel_os/docs/shell.md)
5. [syscall.md](/home/wyh/MiniOS/phase2_kernel_os/docs/syscall.md)

## 17. Phase2 验收测试入口

如果要验证当前 Phase2 是否仍然稳定，建议优先看：

1. [phase2_smoke.md](/home/wyh/MiniOS/phase2_kernel_os/docs/phase2_smoke.md)
2. [phase2_regression.md](/home/wyh/MiniOS/phase2_kernel_os/docs/phase2_regression.md)

## 18. Phase2 冻结说明

到 Task102 为止，Phase2 建议进入：

1. 功能冻结
2. 文档和测试入口冻结
3. `phase2-complete` 节点确认

后续如果继续推进，建议不要再把大功能塞回 Phase2，而是转到：

1. [phase3_plan.md](/home/wyh/MiniOS/phase2_kernel_os/docs/phase3_plan.md)
