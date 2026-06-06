# Task55：Shell 前后台任务观察 / jobs 命令整理

## 1. 本轮目标

本轮目标是新增 shell `jobs` 命令，用当前 shell 的视角观察它通过 `start` 创建的后台任务。

## 2. 为什么需要本任务

`ps` 已经能显示全局进程表，但它不是 shell 的后台任务列表。用户执行：

```text
start loop
```

之后，需要一种更直接的方式查看“当前 shell 管理了哪些后台任务”。这就是 `jobs` 的定位。

## 3. 当前 jobs 语义

当前 `jobs` 的最小语义是：

```text
jobs
    -> 找到当前 shell pid
        -> 遍历 process_info
            -> parent_pid == shell_pid
            -> is_background == 1
                -> 输出 JOB / PID / STATE / NAME
```

没有后台任务时输出：

```text
No background jobs
```

## 4. start 与 jobs 的关系

`start <program>` 会创建后台子进程，并把该进程标记为后台任务。`jobs` 只显示这些后台任务。

前台 `run <program>` 不会进入 `jobs`。

## 5. jobs 与 ps 的区别

- `ps`：系统全局进程表视角，显示 init、shell、用户程序等所有可见进程
- `jobs`：当前 shell 后台任务视角，只显示当前 shell 直接管理的后台子进程

因此 `jobs` 不应该显示 init 和 shell 自己。

## 6. jobs 与 wait / reaper 的关系

`jobs` 只是观察命令，不负责真正释放资源。

后台任务退出或被 kill 后：

1. 进程会进入 `ZOMBIE`
2. `jobs` 可以显示这个状态
3. `wait` 或 init/reaper 回收后，进程槽变为 `UNUSED`
4. `jobs` 不再显示该任务

## 7. jobs 与 kill 的关系

Task54 已经整理了 `kill <pid>`。因此当前可以验证：

```text
start loop
jobs
kill <loop_pid>
jobs
wait
jobs
```

kill 后目标会进入 `ZOMBIE`，wait 回收后从 jobs 中消失。

## 8. 验证方式

建议验证：

```text
jobs
start loop
jobs
ps
run hello
jobs
run echo hello minios
start loop_exit
jobs
wait
jobs
start not_exist_program
jobs
```

## 9. 当前限制

- 暂不支持 `fg`
- 暂不支持 `bg`
- 暂不支持 Ctrl+Z
- 暂不支持 `SIGSTOP/SIGCONT`
- 暂不支持进程组
- 暂不支持 session
- 暂不支持 tty 前台控制
- 当前 `JOB` 是遍历时临时生成的显示编号，不是持久 job id
- 后续可以扩展更完整 job control
