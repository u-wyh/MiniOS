# MiniOS Phase2 fd 文档

## 1. fd 是什么

在 MiniOS Phase2 里，`fd` 是用户进程访问 I/O 资源时使用的最小句柄。

用户态程序并不直接操作：

1. 内置只读文件表
2. RAMFS 文件槽位
3. 教学版 pipe buffer

它们统一通过：

```text
read(fd, ...)
write(fd, ...)
close(fd)
```

进入内核，再由内核根据 `fd` 的类型决定真实访问对象。

## 2. 当前 fd 类型

Task79 之后，当前教学版 fd 表最少能区分以下几类：

1. `FD_NONE`
   表示空槽位，没有绑定资源。
2. `FD_FILE`
   表示普通文件 fd。
   既包括内置只读文本文件，也包括 RAMFS 文本文件。
3. `FD_PIPE_READ`
   表示教学版 pipe 的读端。
4. `FD_PIPE_WRITE`
   表示教学版 pipe 的写端。

当前代码里的常量名是：

```text
PROCESS_FD_TYPE_NONE
PROCESS_FD_TYPE_FILE
PROCESS_FD_TYPE_PIPE_READ
PROCESS_FD_TYPE_PIPE_WRITE
```

## 3. 普通文件 fd 语义

普通文件 fd 仍然沿用 Task58 / Task63 以来的最小语义：

1. `open(path)` 成功后返回一个 `fd >= 3`
2. `read(fd, ...)` 从当前 `offset` 开始读取
3. `write(fd, ...)` 当前只允许写 RAMFS 文件
4. `close(fd)` 释放当前表项

当前不支持：

1. 目录 fd
2. `lseek`
3. `dup`
4. `dup2`
5. 多进程共享同一个真实 file object

## 4. pipe read fd 语义

`pipe read fd` 表示教学版单管道的读端。

最小语义是：

1. 只能读，不能写
2. 读取来源是当前唯一的教学版全局 pipe buffer
3. `read(fd, ...)` 从 `read_offset` 开始读取
4. 读完后返回 `0`，表示最小 EOF
5. 对 `pipe read fd` 调用 `write(fd, ...)` 返回错误

当前它不对应独立 pipe object，也没有引用计数。

## 5. pipe write fd 语义

`pipe write fd` 表示教学版单管道的写端。

最小语义是：

1. 只能写，不能读
2. 写入目标是当前唯一的教学版全局 pipe buffer
3. 写入前会检查剩余容量
4. 写满时沿用 Task78 的“截断 + 单次提示”策略
5. 对 `pipe write fd` 调用 `read(fd, ...)` 返回错误

当前它也不对应独立 pipe object，只是当前 pipe buffer 的一个写端抽象。

## 6. fd=0 / fd=1 / fd=2 的关系

当前 MiniOS 仍然保留教学版标准编号约定：

1. `fd=0`
   表示 `stdin`
2. `fd=1`
   表示 `stdout`
3. `fd=2`
   预留给 `stderr`

但这还不是完整 Linux/Unix 的“标准 fd 都在统一打开文件表里”的实现。

当前更准确的说法是：

1. `fd>=3` 的普通文件 fd 和 pipe fd 真正存放在 `fd_table[]`
2. `fd=0` / `fd=1` 仍然带有教学版特殊入口语义
3. Task79 只是让 `stdin <- pipe` 和 `stdout -> pipe` 开始绑定到 `pipe read fd / pipe write fd`

也就是说，Phase2 现在处在：

```text
保留 stdin/stdout 特殊入口
  +
把 pipe 开始纳入 fd 类型体系
```

这个过渡阶段。

## 7. sys_read / sys_write 如何看待 fd

当前最小分发模型是：

1. `read(fd, ...)`
   - `fd=0`：先看当前进程是否绑定了 `pipe read fd`，否则再看 stdin 文件重定向
   - `FD_FILE`：走普通文件读取
   - `FD_PIPE_READ`：走 pipe buffer 读取
   - `FD_PIPE_WRITE`：返回错误

2. `write(fd, ...)`
   - `FD_FILE`：走普通文件写入
   - `FD_PIPE_WRITE`：走 pipe buffer 写入
   - `FD_PIPE_READ`：返回错误

3. `SYS_WRITE(text)`
   - 当前仍是教学版 stdout 快捷路径
   - 若当前进程启用了 `stdout -> pipe`，最终会转到绑定的 `pipe write fd`
   - 若启用了 stdout 文件重定向，则写 RAMFS
   - 否则输出到前台屏幕

## 8. fd 查询 / 分配 / 清理流程

Task80 之后，当前 fd 路径比 Task79 更清楚了一些。

现在的最小内部流程大致是：

1. `fd_alloc`
   - 在当前进程的 `fd_table[]` 中寻找空闲槽位
   - 普通文件 fd 和 pipe fd 共用这一层分配入口

2. `fd_get`
   - 先把 `fd number` 映射成 `fd_table` 槽位
   - 再返回表项指针
   - `read/write/close` 复用同一套查找入口

3. `fd_reset`
   - 统一清空 `used / type / path / can_write / offset`
   - `close` 和进程初始化都复用这条重置路径

也就是说，本轮没有做独立 `fd.c`，但已经把“查找 / 分配 / 清理”的最小辅助逻辑从散落判断里收了一层。

## 9. fd_dup2 雏形

Task81 之后，内核内部新增了教学版 `fd_dup2(oldfd, newfd)` 雏形。

它的定位是：

1. 只供内核内部使用
2. 当前没有用户态 `dup2` syscall
3. 用于给 redirect / pipe 提供更统一的“把 oldfd 接到 0/1”入口

当前最小语义是：

1. `oldfd` 必须是一个已经打开的教学版 fd 表项
2. `newfd` 当前支持：
   - `0`
   - `1`
   - `>= 3` 的普通 fd 槽位
3. `oldfd == newfd` 时直接成功
4. `newfd` 已占用时，先清空再覆盖
5. `FD_FILE`
   - 复制路径、写标记和 offset 等当前最小字段
6. `FD_PIPE_READ`
   - 可复制到 `fd=0`
   - 也可复制到新的普通 fd 槽位
7. `FD_PIPE_WRITE`
   - 可复制到 `fd=1`
   - 也可复制到新的普通 fd 槽位

错误语义：

1. `oldfd` 无效时返回错误
2. `newfd` 超范围时返回错误
3. 对 `pipe write fd -> fd=0` 返回错误
4. 对 `pipe read fd -> fd=1` 返回错误

关于 offset：

1. `newfd >= 3` 时，当前是“按字段复制”，不是共享 file object
2. 所以 file fd 的 offset 也是“复制值”，不是共享同一个 offset
3. `newfd = 1` 且 `oldfd` 是文件 fd 时，当前仍回落到教学版 stdout 文件重定向语义，不是完整 POSIX 共享写位置

## 10. 当前限制

Task79 之后，fd 抽象比之前更统一了一步，但还远不是完整 Unix/Linux 设计。

当前仍然不支持：

1. fork 后共享 pipe fd
2. 阻塞读写
3. 并发 pipe
4. 多级管道
5. 多个 pipe object
6. socket / tty / 设备文件统一进入同一对象模型

所以现在的定位是：

```text
教学版文件 fd
  +
教学版 pipe fd 雏形
```

而不是完整 VFS + file object + inode + pipe inode 的 Unix 体系。

## 11. 当前保留的兼容路径

为了不破坏现有 Phase2 shell / redirect / pipe 行为，当前仍保留少量过渡性特殊入口：

1. `fd=0`
   仍然是教学版 `stdin` 入口，不是完全从 `fd_table[]` 里直接查出来的标准 fd。
2. `SYS_WRITE(text)`
   仍然是教学版 stdout 快捷路径，不是完整 `write(1, ...)` 模型。
3. `stdout_redirect_to_pipe`
4. `stdin_redirect_from_pipe`

这两个字段现在更多是兼容状态位；真正的 pipe 读写分发已经开始优先看绑定的 `stdin_pipe_fd / stdout_pipe_fd`。

Task81 之后的迁移状态是：

1. `pipe`
   已开始通过内核内部 `fd_dup2(oldfd, 0/1)` 接入标准入口
2. `stdin` 文件重定向
   目前仍保留兼容路径，后续可继续迁移
3. `stdout` 文件重定向
   目前仍保留兼容路径，后续可继续迁移

Task82 之后，这里的状态更新为：

1. `stdin` 文件重定向
   已开始通过：
   - `open(input)`
   - `fd_dup2(input_fd, 0)`
   接入 `fd=0`
2. `stdout` 文件重定向
   已开始通过：
   - `open/create(output)`
   - `fd_dup2(output_fd, 1)`
   接入 `fd=1`
3. `< input > output`
   会分别对 `fd=0` 和 `fd=1` 做两次设置
4. `pipe`
   Task83 之后，shell pipe 连接已经明确统一为：
   - 左侧 `fd_dup2(pipe_write_fd, 1)`
   - 右侧 `fd_dup2(pipe_read_fd, 0)`
   兼容字段仍保留，但主要接线入口已经进入 `fd_dup2`

Task84 之后，用户态 `pipe()` syscall 也开始直接返回一对 pipe fd：

1. `fds[0]`
   - `FD_PIPE_READ`
2. `fds[1]`
   - `FD_PIPE_WRITE`

这说明 pipe 不再只由 shell 内部隐式创建，也可以作为当前进程显式拿到的一对 fd 来测试读写。

Task85 之后，用户态也可以直接调用 `dup2(oldfd, newfd)`：

1. `oldfd`
   - 表示已有 fd
2. `newfd`
   - 表示希望绑定成同一资源入口的新 fd 位置
3. 成功时返回 `newfd`
4. 失败时返回 `-1`
5. `oldfd == newfd` 时直接稳定返回 `newfd`
6. `newfd` 已打开时，当前沿用教学版“先清理再覆盖”语义

当前仍然不是完整 POSIX `dup2`，原因主要是：

1. 没有引用计数
2. `newfd >= 3` 时当前仍是表项复制，不共享同一个 offset 对象
3. 还没有 fork 后共享 fd
4. 还没有 close-on-exec

Task86 之后，fork 也开始继承当前教学版 fd 视图：

1. 子进程会复制父进程 `fd_table[]`
2. 子进程会复制当前 `stdin/stdout` 兼容字段
3. 子进程会复制 `stdin_pipe_fd / stdout_pipe_fd`
4. 当前仍然是浅拷贝 / 视图复制，不是共享同一个底层 file object

Task87 之后，这套 fd 能力已经可以在用户态组合使用：

1. 父进程 `pipe(fds)`
2. `fork()`
3. 子进程 `dup2(fds[1], 1)`
4. 父进程 `dup2(fds[0], 0)`
5. 双方继续只通过 `read/write` 访问资源

这说明当前教学版 fd 已经足以支撑最小“用户态组合 pipe”实验。

Task88 之后，`close` 对不同 fd 的影响也更明确了：

1. 关闭普通文件 fd：
   - 当前仍只是清理当前进程自己的 fd 表项
   - 不涉及共享 file object 或引用计数
2. 关闭 `FD_PIPE_READ`：
   - 会标记教学版 pipe 读端关闭
   - 不主动丢弃 buffer 中已有数据
3. 关闭 `FD_PIPE_WRITE`：
   - 会标记教学版 pipe 写端关闭
   - 读端之后仍可把已有数据读完
4. 重复 close：
   - 当前第二次 close 返回错误
   - 但不会 panic
5. 当前 fd 表清理仍然没有引用计数：
   - 任意一个 pipe 端被 close，就会直接影响这条教学版全局 pipe 的对应端状态
   - 这不是完整 POSIX 多引用语义

Task89 之后，当前教学版 `exec` 与 fd table 的关系也更明确了：

1. `exec` 默认保留当前进程 fd table
2. `fd=0 / fd=1` 在 exec 后不会被重置
3. 普通文件 fd 在 exec 后仍然可继续读写
4. `FD_PIPE_READ / FD_PIPE_WRITE` 在 exec 后仍然可继续使用
5. 当前没有 close-on-exec：
   - exec 时不会自动关闭某些 fd
   - 这仍然不是完整 POSIX 语义

Task90 之后，当前 fd 抽象已经足以在用户态自己组合最小 pipeline demo：

1. 父进程 `pipe(fds)`
2. writer 子进程 `dup2(fds[1], 1)`
3. writer 子进程 `exec(pipeline_writer)`
4. reader 子进程 `dup2(fds[0], 0)`
5. reader 子进程 `exec(pipeline_reader)`
6. `pipeline_writer` 只通过 `write(1, ...)` 写数据
7. `pipeline_reader` 只通过 `read(0, ...)` 读数据

这说明当前教学版系统已经具备最小“标准输入输出只是 fd 入口，背后资源可被重新接线”的演示能力。

Task92 之后，这套 fd 能力已经可以继续承接带参数 consumer：

1. writer 子进程 `dup2(pipe_write_fd, 1)` 后 `exec(pipeline_writer)`
2. consumer 子进程 `dup2(pipe_read_fd, 0)` 后 `exec(grep, argc=2, argv={"grep","MiniOS"})`
3. `grep` 继续只通过：
   - `read(0, ...)`
   - `SYS_GET_ARGC / SYS_GET_ARG`
   读取 stdin 和自己的 argv

这说明当前教学版 fd 不仅能承接“固定程序接线”，也已经能承接“带参数程序接线”。

Task94 之后，这套 fd 能力又进一步收敛成 `mini_pipeline` 这个用户态固定命令入口：

1. 左侧子进程：
   - `dup2(pipe_write_fd, 1)`
   - `exec(left_prog)`
2. 右侧子进程：
   - `dup2(pipe_read_fd, 0)`
   - `exec(right_prog, argv)`
3. 右侧程序仍然只通过：
   - `read(0, ...)`
   - `SYS_GET_ARGC / SYS_GET_ARG`
   工作

这说明当前教学版 fd 已经不只是 shell 内部接线或 demo 测试，而是开始具备一个最小可复用的用户态 pipeline 命令入口。

Task95 继续把这条入口补成“双端 argv”模型：

1. 左侧子进程：
   - `dup2(pipe_write_fd, 1)`
   - `exec(left_prog, left_argv)`
2. 右侧子进程：
   - `dup2(pipe_read_fd, 0)`
   - `exec(right_prog, right_argv)`

这说明当前教学版 fd 已经能同时承接：

1. 左侧带参数文件读取程序
2. 右侧带参数文本处理程序

Task96 之后，这套 fd 接线又往真实 pipeline 靠近了一步：

1. 左侧子进程先通过 `dup2(pipe_write_fd, 1)` 把 `fd=1` 接到 pipe 写端
2. 右侧子进程先通过 `dup2(pipe_read_fd, 0)` 把 `fd=0` 接到 pipe 读端
3. 双方都已经存在时，pipe 读写不再要求“左侧全部结束后右侧再开始”

当前仍然是教学版实现：

1. 仍然只有一个全局 pipe buffer
2. 当前等待是 busy-wait，不是完整阻塞队列
3. 但 file fd / pipe fd / dup2 / exec 的组合已经足以跑最小并发 pipeline

Task97 之后，fd 与 pipe 的关系又更清晰了一步：

1. `struct process_fd_entry` 现在直接保存 `pipe_id`
2. `FD_PIPE_READ / FD_PIPE_WRITE` 不再直接代表“那一份全局 pipe 数据”
3. 它们只表示：
   - 这个 fd 是读端还是写端
   - 它绑定到哪一个 `pipe_id`

也就是说：

```text
fd 3 -> FD_PIPE_READ  -> pipe_id = 0
fd 4 -> FD_PIPE_WRITE -> pipe_id = 0
fd 5 -> FD_PIPE_READ  -> pipe_id = 1
fd 6 -> FD_PIPE_WRITE -> pipe_id = 1
```

真正的数据状态保存在：

```text
pipe_table[pipe_id]
```

当前 `dup2` / `fork` 对 pipe fd 的影响也可以更精确地描述为：

1. `dup2(pipe_fd, newfd)`
   - 复制 pipe fd 表项
   - 同时复制 `pipe_id`
   - 不会新建 pipe object
2. `fork()`
   - 子进程复制父进程 `fd_table[]`
   - pipe fd 继承时也只是继承同一个 `pipe_id`

当前仍然没有完整 POSIX file object / 引用计数模型，但 fd 已经不再直接“挂在一份全局 pipe 数据上”，而是通过 `pipe_id` 间接访问 pipe object。
