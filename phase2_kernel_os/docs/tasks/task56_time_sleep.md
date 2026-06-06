# Task56：系统 tick / sleep / uptime 语义整理

## 1. 本轮目标

本轮目标是把 MiniOS 里 `PIT -> ticks -> sleep / wakeup -> uptime` 这条最小时间链路整理清楚，并让用户能通过 `uptime`、`sleep`、`sleep_test` 观察时间推进。

## 2. 为什么需要本任务

系统时间不是单独的“显示功能”，它会直接影响：

- 调度统计
- 进程睡眠与唤醒
- `ps` 中的 `AGE / RUNS`
- shell 对 `uptime` 的观察能力

如果 tick 语义不清楚，后续的 `sleep`、进程状态和运行统计就都不稳定。

## 3. PIT 与 ticks

当前 MiniOS 继续使用 PIT 产生周期性 IRQ0：

```text
PIT IRQ0
    -> timer_handler
        -> ticks++
            -> 唤醒到期睡眠进程
            -> 按当前策略驱动最小调度
```

本轮统一复用：

- `pit_get_ticks()`：读取累计 tick
- `pit_get_frequency()`：读取当前 PIT 频率

当前默认频率是 `20Hz`，因此：

- `1 second = 20 ticks`
- `1 tick ≈ 50ms`

## 4. sleep 语义

当前 `sleep(n)` 的参数单位是 tick，不是秒。

最小语义是：

```text
sleep(n)
    -> 当前进程进入 SLEEPING
        -> wakeup_tick = now + n
            -> 调度器跳过该进程
                -> 到期后恢复为 READY
```

补充约定：

- `sleep(0)` 当前退化为最小 `yield`
- 睡眠进程不会在睡眠期间持续占用 CPU
- 唤醒后只是回到 `READY`，等待后续调度

## 5. uptime 命令

当前 shell 支持：

```text
uptime
ticks
```

输出会显示：

- 累计 tick 数
- 按 `20Hz` 做的最小 seconds 换算

这仍然只是教学版 uptime，不表示真实日期时间。

## 6. 与 scheduler 的关系

Task56 没有重写调度器，只明确了当前配合关系：

- PIT tick 周期性推进系统时间
- `process_wakeup_sleeping()` 在 tick 中检查到期睡眠进程
- `SLEEPING` 进程不会被当作可运行进程选择
- `AGE` 继续随 tick 增长
- `RUNS` 不会因为进程睡眠而异常增长

## 7. 验证方式

本轮主要通过以下命令验证：

```text
uptime
sleep 100
uptime
run sleep_test
start sleep_test
ps
```

重点观察：

1. `uptime` 数值会递增
2. `sleep_test` 会打印 sleep 前后 tick
3. `ps` 能显示 `SLEEPING` 等合理状态
4. 睡眠到期后进程能继续执行

## 8. 当前限制

1. 暂不支持 RTC 真实日期时间
2. 暂不支持时区
3. 暂不支持 wall clock
4. 暂不支持高精度定时器
5. 暂不支持 `nanosleep`
6. 暂不支持 `timerfd`
7. 暂不支持信号唤醒
8. 后续可以扩展更完整的定时器与时间子系统
