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

## 8. Task89：fork 与 exec 的区别

Task89 重点整理了当前教学版 `exec` 会替换什么、保留什么：

1. `fork`：
   - 创建一个新的进程对象
   - 复制父进程当前的 fd 视图
   - 子进程继续从相同的用户态返回点执行
2. `exec`：
   - 不创建新进程
   - 只替换当前进程的用户镜像与返回现场
   - 默认保留当前 fd table
   - 不主动重置 `fd=0 / fd=1`

因此当前 MiniOS 已经具备最小语义：

1. `dup2(pipe_write_fd, 1)`
2. `exec(writer_program)`
3. writer 程序继续通过 `write(1, ...)` 把数据写进 pipe

这一步已经足以为后续用户态 pipeline demo 铺路，但仍然不是完整 POSIX exec。

## 9. Task90：用户态 pipeline demo

Task90 没有继续大改 `fork` 或 `exec` 主体，而是新增用户态 `pipeline_demo` / `pipeline_writer` / `pipeline_reader` 来验证：

1. 父进程 `pipe(fds)`
2. 父进程 `fork()` writer 子进程
3. writer 子进程 `dup2(fds[1], 1)`
4. writer 子进程 `exec(pipeline_writer)`
5. 父进程 `waitpid(writer)`
6. 父进程 `fork()` reader 子进程
7. reader 子进程 `dup2(fds[0], 0)`
8. reader 子进程 `exec(pipeline_reader)`
9. 父进程 `waitpid(reader)`

这个 demo 说明：

1. `fork` 后 fd 继承已经足以支撑父子两端拿到同一对 pipe fd
2. `exec` 后 `fd=0 / fd=1` 绑定关系仍然保留
3. 当前虽然还是教学版顺序模型，但已经能在用户态自己拼出最小 `producer | consumer` 链路

## 10. Task91：exec 的 argc / argv 语义

Task91 没有重写 `exec` 主体，而是把当前教学版参数传递路径补齐说明并新增验证程序：

1. `SYS_EXEC_ARGS(program_id, argc, argv)`
2. 内核把参数复制到当前进程 PCB 暂存区
3. `exec` 替换当前用户镜像
4. 新程序通过：
   - `SYS_GET_ARGC`
   - `SYS_GET_ARG`
   读取自己的教学版 `argc / argv`

当前 `exec` 会替换：

1. 当前进程用户镜像
2. 返回现场
3. 当前程序名

当前 `exec` 会保留：

1. 当前进程 pid
2. fd table
3. `fd=0 / fd=1` 的教学版绑定关系
4. pipe / redirect 已接好的数据流状态

当前与 POSIX `execve` 的主要差距是：

1. 没有 `envp`
2. 没有完整用户栈参数布局
3. `argv` 当前仍保存在 PCB 暂存区里
4. 参数数量和长度使用固定上限

## 11. Task92：带参数 pipeline demo

Task92 没有继续大改 `fork` 或 `exec` 主体，而是新增 `pipeline_args_demo` 来验证：

1. writer 子进程能像 Task90 一样执行 `dup2(pipe_write_fd, 1)` 后 `exec(pipeline_writer)`
2. consumer 子进程能执行 `dup2(pipe_read_fd, 0)` 后 `exec(grep, argc=2, argv={"grep","MiniOS"})`
3. `grep` 在 `exec` 后既能保留 `fd=0`，又能收到自己的 `argv[1]`

这个 demo 说明：

1. `fork` 后 fd 继承语义仍然足以支撑 pipeline 两端
2. `exec` 后 fd 保留和 argv 传递已经能同时工作
3. 当前教学版 pipeline 已经能运行“带参数 consumer”这一类更接近真实 shell 的场景

## 12. Task94：mini_pipeline 命令

Task94 没有继续重写 `fork` 或 `exec` 主体，而是新增 `mini_pipeline` 来把现有能力收敛成一个更像命令入口的用户态程序：

1. `pipe(fds)`
2. `fork()` 左侧 writer 子进程
3. 左侧子进程 `dup2(fds[1], 1)` 后 `exec(left_prog)`
4. 父进程 `waitpid(writer)`
5. `fork()` 右侧 consumer 子进程
6. 右侧子进程 `dup2(fds[0], 0)` 后 `exec(right_prog, argv)`
7. 父进程 `waitpid(consumer)`

它验证的是：

1. `fork` 继续能复制当前教学版 fd 视图
2. `dup2` 继续能把 pipe 端点接到 `fd=0 / fd=1`
3. `exec` 继续能保留这层 fd 绑定
4. 右侧程序继续能收到自己的 `argv`

当前仍然是教学版顺序 pipeline，不依赖并发阻塞 pipe。

## 13. Task95：mini_pipeline 双端 argv

Task95 没有去改 `fork`、`dup2` 或 `exec` 主机制，而是把 `mini_pipeline` 自己的左右两侧参数切分规则补齐：

1. `--` 左边生成 `left_argc / left_argv`
2. `--` 右边生成 `right_argc / right_argv`
3. 左侧子进程 `dup2(fds[1], 1)` 后执行 `exec(left_prog, left_argv)`
4. 右侧子进程 `dup2(fds[0], 0)` 后执行 `exec(right_prog, right_argv)`

这说明当前教学版 process 路线已经不只是能承接“固定 writer/reader”，也已经能承接：

1. 左侧带参数 producer
2. 右侧带参数 consumer

例如：

1. `run mini_pipeline cat /readme.txt -- grep MiniOS`
2. `run mini_pipeline cat /readme.txt -- head -n 3`

## 14. Task96：并发 mini_pipeline

Task96 没有重写 scheduler 或 `exec` 主体，而是把 `mini_pipeline` 自身的流程推进成最小并发模型：

1. `pipe(fds)`
2. `fork()` 左侧 writer
3. `fork()` 右侧 reader
4. 左侧 `dup2(fds[1], 1)` 后 `exec(left_prog, left_argv)`
5. 右侧 `dup2(fds[0], 0)` 后 `exec(right_prog, right_argv)`
6. 父进程关闭自己的 pipe 端点，再分别 `waitpid()`

这说明当前 process 路线已经能支撑：

1. 父进程只负责搭线和回收
2. 左右两个子进程并存
3. 数据通过单全局教学版 pipe 在二者之间流动

## 15. Task97：fork / exit 与 pipe_id 生命周期

Task97 没有重写 `fork`、`dup2` 或 `exit` 主机制，而是把它们与 pipe object 的关系整理成：

1. `fork()`
   - 子进程复制父进程 `fd_table[]`
   - pipe fd 继承时复制的是同一个 `pipe_id`
   - 当前仍然不是完整引用计数模型
2. `dup2()`
   - 复制 pipe fd 时也只是复制 `pipe_id`
   - 不会新建 pipe object
3. `close()` / `exit`
   - 关闭一个 pipe fd 后，只更新对应 `pipe_id` 那个对象的 `read_open / write_open`
   - 不会把别的 pipe object 一起影响掉
   - 当对应 pipe object 的读写两端都关闭后，对象槽位才会被回收

这说明当前 process 维度已经从：

1. “所有 pipe 都共用一份全局状态”

推进到了：

1. “每个进程里的 pipe fd 只保存 `pipe_id`”
2. “真正的数据和开关状态在 `pipe_table[pipe_id]`”
3. “fork/dup2/close/exit` 都围绕这个 `pipe_id` 工作”
