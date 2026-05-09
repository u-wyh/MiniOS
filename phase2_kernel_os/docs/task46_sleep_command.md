# Task46：用户态 sleep 命令雏形

## 1. sleep 命令和 sleep syscall 的关系是什么

当前 `sleep <ticks>` 命令只是用户态 shell 对已有 `sleep(ticks)` syscall 的最小封装。  
shell 负责：

- 读一行命令
- 拆分参数
- 解析 `ticks`
- 调用 `SYS_SLEEP`

内核负责：

- 把当前 shell 进程改成 `SLEEPING`
- 记录 `wakeup_tick`
- 在 PIT tick 到期后把 shell 改回 `READY`

## 2. sleep <ticks> 为什么使用 tick 作为单位

因为当前 MiniOS 已经有：

- PIT 周期 tick
- `sleep(ticks)` syscall
- `uptime / ticks` 只读查询接口

直接使用 tick 能最小复用现有时间基础，不必在这一轮引入秒级换算和 RTC。

## 3. shell 调用 sleep 后，为什么 shell 自己会暂停

执行命令的是 shell 进程本身。  
所以 `sleep 100` 的真实含义是：

“让当前 shell 进程睡眠 100 个 tick”

这不是让某个外部子进程睡眠，而是让当前前台命令解释器自己暂停。

## 4. shell 睡眠期间为什么不能继续处理命令

因为 shell 在睡眠期间已经不再运行命令循环。  
它既不会继续读键盘，也不会继续解析命令，直到 PIT tick 到期把它唤醒。

## 5. PIT tick 如何唤醒 shell

最小链路如下：

1. shell 调用 `SYS_SLEEP`
2. 内核记录 `wakeup_tick = now + ticks`
3. shell 状态改为 `SLEEPING`
4. PIT 每次 tick 递增系统节拍
5. 到期后把 shell 状态改回 `READY`
6. 调度器再次选中 shell
7. sleep syscall 返回，shell 继续主循环

补充说明：当前教学版系统如果暂时只有 `init + shell` 两个活动进程，没有其他 READY 进程可切换，
shell 命令层会回退到基于 `get_ticks` 的最小等待，优先保证 `sleep <ticks>` 的命令语义可用。

## 6. uptime 如何验证 sleep 的效果

可以直接执行：

```text
uptime
sleep 100
uptime
```

如果第二次 `uptime` 的 tick 值明显大于第一次，并且差值通常不小于约 100，就说明 `sleep <ticks>` 生效了。

## 7. 当前 sleep 命令和 Linux sleep 有什么差距

当前实现仍然是教学版最小模型：

- 单位是 tick，不是秒
- 不支持 `sleep 1s` / `sleep 1m`
- 不支持高精度时间
- 不支持信号中断 sleep
- 不支持复杂阻塞队列

因此它更接近“把当前进程挂起若干个定时节拍”，而不是 Linux 的完整时间接口。

## 8. 后续要支持秒级 sleep / 可中断 sleep 还缺什么

后续如果要继续扩展，还需要补：

- tick 到秒/毫秒的换算
- 更统一的用户态时间 API
- 可中断 sleep 语义
- 更完整的阻塞/唤醒队列
- 更高精度的定时器基础设施
