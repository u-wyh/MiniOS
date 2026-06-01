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

## 8. 当前限制

Task79 之后，fd 抽象比之前更统一了一步，但还远不是完整 Unix/Linux 设计。

当前仍然不支持：

1. 用户态 `pipe()` syscall
2. `dup2()`
3. fork 后共享 pipe fd
4. 阻塞读写
5. 并发 pipe
6. 多级管道
7. 多个 pipe object
8. socket / tty / 设备文件统一进入同一对象模型

所以现在的定位是：

```text
教学版文件 fd
  +
教学版 pipe fd 雏形
```

而不是完整 VFS + file object + inode + pipe inode 的 Unix 体系。
