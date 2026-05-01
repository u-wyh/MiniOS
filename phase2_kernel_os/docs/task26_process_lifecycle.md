# Task26：进程生命周期管理

## 1. 本任务目标

本轮让进程具备最小生命周期：创建、运行、退出、进入僵尸状态，并由 shell 的 `wait` 命令回收 PCB。

## 2. 核心知识点

进程生命周期描述一个进程从创建到被回收的完整状态变化。

ZOMBIE 进程表示用户程序已经退出，但 PCB 仍保留在进程表中，等待内核或 shell 回收它的退出信息。

ZOMBIE 和 UNUSED 的区别：

- `PROCESS_ZOMBIE`：进程已经退出，但 PID、退出码、名称等记录仍可被 `ps` 和 `wait` 观察。
- `PROCESS_UNUSED`：PCB 槽位空闲，可以被新的进程复用。

`exit` 做的事情：

- 保存退出码。
- 把当前进程状态改为 `PROCESS_ZOMBIE`。
- 请求离开用户态，回到内核控制台。

`wait` 做的事情：

- 扫描进程表。
- 找到一个 ZOMBIE 进程。
- 打印并返回它的 pid。
- 把 PCB 清回 `PROCESS_UNUSED`。

exit 后不能立即完全删除 PCB，是因为 shell 还需要通过 `wait` 观察退出结果。本轮没有父子进程关系，所以 `wait` 简化为回收任意一个 ZOMBIE。

## 3. 当前生命周期模型

```text
--------+      +-------+      +---------+      +--------+
| UNUSED | --> | READY | --> | RUNNING | --> | ZOMBIE |
+--------+      +-------+      +---------+      +--------+
    ^                                             |
    |                                             |
    +---------------- wait -----------------------+
```

状态流转：

```text
UNUSED -> READY -> RUNNING -> ZOMBIE -> UNUSED
```

## 4. 执行流程

```text
用户程序 exit
-> int 0x80
-> syscall handler
-> process_exit
-> 标记 ZOMBIE
-> 离开用户态
-> shell wait
-> process_wait
-> 回收 PCB
```

调度器和执行入口必须跳过 ZOMBIE 进程，避免已经退出的用户代码再次运行。

## 5. 关键代码解释

`process_exit(status)`：

- 获取当前进程。
- 保存 `exit_status`。
- 把状态设置为 `PROCESS_ZOMBIE`。
- 清空 `current_process`，表示没有用户进程正在运行。

`process_wait()`：

- 遍历进程表。
- 找到第一个 `PROCESS_ZOMBIE`。
- 保存 pid 后清空 PCB。
- 返回被回收的 pid；没有 ZOMBIE 时返回 `-1`。

`process_state_name(state)`：

- 把状态码转换成 `UNUSED / READY / RUNNING / ZOMBIE` 字符串，供 `ps` 输出。

`SYS_EXIT`：

- 用户态通过 `eax = SYS_EXIT`、`ebx = exit_status` 调用。
- 内核保存退出码并把进程标记为 ZOMBIE。

`ps`：

- 遍历进程表。
- 跳过 `PROCESS_UNUSED`。
- 显示 PID、STATE、退出码和进程名。

## 6. 当前限制

- 没有 fork。
- 没有父子进程关系。
- `wait` 回收任意 ZOMBIE，而不是只等待子进程。
- 没有完整资源释放。
- 没有 waitpid。
- 没有进程退出通知机制。

## 7. 常见错误

- exit 后进程还继续运行。
- 调度器没有跳过 ZOMBIE。
- wait 直接删除 RUNNING 进程。
- ps 无法显示 ZOMBIE。
- 没有保存 exit_status。
- wait 后 PCB 没有变回 UNUSED。

## 8. 一句话总结

进程生命周期管理的本质是：让进程从创建、运行、退出到回收形成闭环。
