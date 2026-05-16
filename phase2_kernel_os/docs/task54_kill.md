# Task54：kill syscall / shell kill 命令整理

## 1. 本轮目标

本轮目标是整理 MiniOS Phase2 当前已有的 `SYS_KILL` 和 shell `kill <pid>`，让后台用户进程可以通过 pid 被安全终止，并继续复用 `exit / wait / reaper` 生命周期。

## 2. 为什么需要本任务

`start loop` 这类后台程序会长期运行。如果没有清晰的 kill 语义，用户只能观察进程，却不能主动控制它。

Task54 让 MiniOS 具备最小进程控制闭环：

```text
start loop
    -> ps
        -> kill <pid>
            -> wait
                -> ps
```

## 3. 当前 kill 语义

当前 `kill(pid)` 不是 Unix/Linux 信号系统，而是教学版“按 pid 终止进程”接口。

成功时：

1. 通过 pid 找到目标进程
2. 检查目标是否允许终止
3. 写入 `PROCESS_KILL_EXIT_STATUS`
4. 把目标状态改为 `PROCESS_ZOMBIE`
5. 后续交给 `wait` 或 init/reaper 回收

## 4. SYS_KILL 说明

- syscall 编号：`SYS_KILL = 13`
- 参数：`ebx = pid`
- 返回值：
  - 成功返回 `0`
  - 目标不存在、目标是 init、目标是当前 shell、目标已经退出等情况返回负值

`PROCESS_KILL_EXIT_STATUS` 只表示“被 kill 终止”，不是 `SIGKILL` 编号。

## 5. shell kill 命令

使用方式：

```text
kill <pid>
```

当前 shell 行为：

- 缺少 pid：输出 `Usage: kill <pid>`
- pid 非数字：输出 `Invalid pid`
- kill 成功：输出 `Killed`
- kill 失败：输出 `Kill failed`

## 6. kill 后生命周期

kill 后仍然复用 Task52 / Task53 的生命周期：

```text
running / ready / sleeping
    -> killed / exited
        -> zombie
            -> wait / reap
                -> free slot
```

因此 kill 不直接释放资源，避免破坏父进程读取退出状态和统一回收逻辑。

## 7. 安全限制

- 不允许 kill init
- 不允许当前 shell 直接 kill 自己
- 不支持 `kill -9`
- 不支持信号编号
- 不支持进程组 kill
- 不支持权限模型

## 8. 验证方式

建议使用：

```text
start loop
ps
kill <loop_pid>
ps
wait
ps
```

额外边界：

```text
kill 9999
kill abc
kill 1
kill <shell_pid>
run hello
run echo hello minios
run sleep_test
run not_exist_program
```

## 9. 当前限制

- 暂不支持完整信号系统
- 暂不支持 `kill -9`
- 暂不支持进程组 kill
- 暂不支持权限检查
- 暂不支持复杂 job control
- 后续可以扩展 signal / jobs / fg / bg 等更完整进程控制能力
