# Task52：用户程序退出状态 / wait 语义整理

## 1. 本轮目标

本轮目标是把用户程序退出、僵尸保留、父进程等待回收、init/reaper 补位回收这条最小生命周期闭环整理清楚。

## 2. 为什么需要本任务

Task50 和 Task51 已经把“如何启动一个用户程序、如何传递参数”收口得比较清楚了。

接下来如果不把：

```text
exit(status) -> zombie -> wait/reap
```

这条路径整理好，后台任务和 `ps` 的语义就会越来越难维护。

## 3. 当前进程生命周期

当前 MiniOS 采用教学版最小模型：

```text
create / exec
    -> ready
        -> running
            -> exit(status)
                -> ZOMBIE
                    -> wait / reap
                        -> free slot
```

这里不追求完整 Linux 行为，只要求：

- 退出进程不再继续运行
- 父进程还有机会看到它的退出结果
- 最终能被回收，不无限堆积

## 4. exit(status) 语义

当前用户程序调用 `SYS_EXIT` 后：

1. 内核记录 `exit_status`
2. 当前进程状态改成 `PROCESS_ZOMBIE`
3. 当前进程不再作为 `RUNNING/READY` 被调度
4. 若父进程正阻塞等待它，内核会恢复父进程
5. 若没有立即被父进程回收，则先保留在进程表里，等待后续 `wait` 或 init/reaper 处理

## 5. wait 语义

当前保留两条最小路径：

- `wait`：回收任意一个已经退出的子进程；如果当前没有已退出子进程，则返回“没有可回收目标”
- `wait <pid>`：针对指定子进程执行当前最小 `waitpid` 语义

当前 `wait` 主要返回被回收子进程的 `pid`。  
`exit_status` 当前主要保留在 PCB 中，并可通过 `ps` 的 `EXIT` 列观察。

## 6. run / start / wait 行为

- `run <program>`：前台运行，shell 会等待该子进程退出
- `start <program>`：后台运行，shell 立即返回提示符
- `wait`：手动回收已经退出的后台子进程
- `wait <pid>`：针对指定子进程等待/回收

因此：

- `run loop_exit` 适合验证“前台退出后 shell 恢复”
- `start loop_exit` + `wait` 适合验证“后台退出后手动回收”

## 7. ps 状态说明

当前 `ps` 重点关注这些状态：

- `READY`
- `RUNNING`
- `BLOCKED`
- `SLEEPING`
- `ZOMBIE`

其中：

- `ZOMBIE` 表示进程已经退出，但还没有被父进程或 reaper 回收
- `EXIT` 列显示当前记录的退出码，便于观察正常退出和 `kill` 后的差异

## 8. 验证方式

- `run loop_exit`
- `run hello`
- `run echo hello minios`
- `start loop_exit`
- `wait`
- `ps`
- `start loop`
- `run sleep_test`

这些场景可以覆盖：

- 正常退出
- 参数传递兼容
- 后台退出
- 手动回收
- `ps` 状态观察

## 9. 当前限制

1. 暂不实现完整 Linux `waitpid`
2. 暂不支持信号
3. 暂不支持进程组 / session / TTY 控制
4. 暂不提供复杂父子权限检查
5. `exit_status` 当前主要通过 `ps` 观察，而不是完整 wait 状态对象
6. 后续仍可继续扩展更真实的进程树和 wait 语义
