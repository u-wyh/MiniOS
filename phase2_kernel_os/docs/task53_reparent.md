# Task53：进程父子关系 / reparent 语义整理

## 1. 本轮目标

本轮目标是把 MiniOS Phase2 中的父子进程关系整理清楚，让 `parent_pid`、`wait`、`reparent`、init/reaper 和 `ps` 的语义能够形成闭环。

## 2. 为什么需要本任务

Task52 已经整理了 `exit -> ZOMBIE -> wait/reap` 的生命周期，但如果父进程先退出，子进程就可能继续指向一个已经退出或即将回收的父进程。

Task53 解决的是“父进程不在了，子进程归谁管”的问题。

## 3. 当前进程树语义

当前教学版进程树是：

```text
init
    -> shell
        -> hello / echo / loop / loop_exit / sleep_test
```

这不是完整 Linux 进程树，只是通过 PCB 中的 `parent_pid` 记录最小归属关系。

## 4. parent_pid 约定

- `PROCESS_ROOT_PARENT_PID = 0`
- init 是进程树根，因此 init 的 `PPID` 为 `0`
- shell 由 init 创建，因此 shell 的 `PPID` 指向 init
- shell 启动的用户程序由 shell 创建，因此普通用户程序的 `PPID` 指向 shell

## 5. reparent to init

父进程退出或被 `kill` 时，内核会扫描进程表，把仍有效的子进程转交给 init：

```text
child.parent_pid = init_pid
```

这一步不会直接停止仍在运行的子进程，也不会改写它们的程序镜像，只是修正后续回收责任。

## 6. wait / reaper 规则

- 普通 `wait` 只回收当前进程名下已经退出的子进程
- `waitpid(pid)` 必须确认目标进程是当前进程的子进程
- `wait_any()` 只扫描当前进程名下的 `ZOMBIE`
- init/reaper 只兜底回收已经挂到 init 名下、且没有父进程正在等待的孤儿 `ZOMBIE`

这样可以避免 shell 误回收非子进程，也能避免孤儿 zombie 长期堆积。

## 7. ps 显示

`ps` 中的 `PPID` 列用于观察父子关系：

- init 的 `PPID` 应为 `0`
- shell 的 `PPID` 应指向 init
- `start loop` 后，loop 的 `PPID` 应指向 shell
- 父进程退出后，孤儿进程的 `PPID` 会变为 init

## 8. 验证方式

- `ps`：观察 init / shell 的 `PID` 与 `PPID`
- `run hello`：验证前台子进程仍能正常运行和回收
- `run echo hello minios`：验证 Task51 参数传递不受影响
- `start loop` 后 `ps`：验证后台进程的 `PPID` 指向 shell
- `start loop_exit` 后 `wait`：验证后台退出进程仍能由 shell 回收
- `wait`：验证没有已退出子进程时不会误回收无关进程

## 9. 当前限制

- 暂不实现完整 Linux `waitpid`
- 暂不支持信号系统
- 暂不支持进程组、session 和 TTY 控制
- 暂不维护复杂子链表，只通过 `parent_pid` 扫描进程表
- 后续可以扩展更完整进程树、权限检查和 waitpid 选项
