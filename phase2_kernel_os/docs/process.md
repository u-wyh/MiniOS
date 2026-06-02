# MiniOS Phase2 进程与 fork 说明

## 1. 当前定位

当前 MiniOS Phase2 的进程模型仍然是教学版最小实现。

它已经具备：

1. `fork`
2. `waitpid`
3. `exec`
4. 最小 fd table
5. shell redirect / pipe

但还没有完整 Unix/Linux 那套 file object、引用计数、阻塞 pipe 和并发调度语义。

## 2. Task86：fork 后 fd 继承语义

Task86 的目标，是把 fork 从“只复制用户镜像和返回现场”推进到“复制当前进程看到的 fd 视图”。

当前最小继承内容包括：

1. `fd_table[]`
2. `stdin_redirect_*`
3. `stdout_redirect_*`
4. `stdin_pipe_fd`
5. `stdout_pipe_fd`
6. `stdout_redirect_to_pipe`
7. `stdin_redirect_from_pipe`

## 3. 当前语义

当前 fork 后：

1. 子进程会得到父进程 fd table 的教学版浅拷贝
2. 普通文件 fd 会被复制
3. pipe read fd / pipe write fd 会被复制
4. `fd=0 / fd=1` 对应的当前绑定关系也会被复制

这意味着：

1. 子进程可以继续读父进程已经打开的文件 fd
2. 子进程可以继续使用父进程已经创建的 pipe fd
3. 父子都还能保留自己的 fd 编号入口

## 4. 当前限制

这还不是完整 POSIX fork fd 继承，主要限制是：

1. 没有引用计数
2. 没有共享同一个底层 file object
3. 当前更像“复制一份教学版 fd 视图”
4. pipe 仍然只有一个全局教学版缓冲区
5. 不支持并发 pipe / 阻塞读写

## 5. fork_fd_test

Task86 新增了 `fork_fd_test`，用来验证最小父子 pipe 继承链路：

1. 父进程 `pipe(fds)`
2. 父进程 `fork()`
3. 子进程使用继承到的 `fds[1]` 写 pipe
4. 子进程退出
5. 父进程 `waitpid()`
6. 父进程从 `fds[0]` 读回数据

这个测试说明：

1. 子进程已经继承 pipe fd
2. 子进程退出不会自动清空父进程之后还要读取的 pipe 数据

## 6. Task87：fork 继承后的组合验证

Task87 没有继续修改 fork 主体，而是新增用户态 `pipe_fork_dup2_test` 来验证：

1. 父进程 `pipe(fds)`
2. 父进程 `fork()`
3. 子进程通过继承下来的 pipe write fd 执行 `dup2(fds[1], 1)`
4. 子进程往 `stdout` 写入消息
5. 父进程 `waitpid()` 后执行 `dup2(fds[0], 0)`
6. 父进程从 `stdin` 读回消息

这说明 Task86 的 fd 继承语义已经能够继续支撑更接近真实 UNIX 管道模型的组合测试。

## 7. Task88：exit 与 fd 清理

Task88 没有重写完整进程生命周期，而是补齐了当前教学版 `exit` 前的 fd 清理动作：

1. 进程 `exit` 前，会先关闭自己 still-open 的教学版 fd
2. 关闭 pipe fd 时，只更新当前教学版全局 pipe 的端状态，不主动清空已有数据
3. 因为当前没有引用计数，这仍然不是完整 POSIX 生命周期模型
4. 子进程退出后，不应错误清空父进程稍后还要读取的 pipe buffer 数据

所以当前更准确地说，是“当前进程退出时清理自己的 fd 视图”，而不是完整 UNIX close/release 语义。
