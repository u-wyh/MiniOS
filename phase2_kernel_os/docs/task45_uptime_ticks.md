# Task45：用户态 uptime / ticks 命令雏形

## 1. 目标

在已有 PIT tick 基础上，增加一个最小只读 syscall，允许用户态 shell 查询“系统自启动以来累计的 tick 数”，并通过 `uptime` / `ticks` 命令显示出来。

## 2. 当前实现

- 复用 `pit.c` 中已有的 tick 计数，不新增第二套时间变量。
- 新增 `SYS_GET_TICKS`，只读返回当前累计 tick。
- 用户态 shell 新增 `uptime` 命令。
- shell 同时支持 `ticks` 作为 `uptime` 的别名。

输出格式保持最小：

```text
ticks: <number>
```

## 3. PIT tick 是什么

PIT tick 是可编程定时器 IRQ0 周期中断驱动下递增的系统节拍。  
它不是“现在几点”，而是“系统从启动到现在收到了多少次时钟中断”。

## 4. tick 和真实时间有什么区别

当前 MiniOS 只有 tick，没有 RTC、日历时间和时区支持。  
所以 `uptime` 现在显示的是内核节拍计数，而不是秒、分钟或日期。

## 5. sleep(ticks) 为什么依赖 tick

Task44 的 `sleep(ticks)` 本质上就是：

1. 记录一个未来的 `wakeup_tick`
2. PIT 每次 tick 到来时检查是否到期
3. 到期后把进程从 `SLEEPING` 改回 `READY`

因此，tick 查询接口也正好能帮助我们观察 sleep 是否按预期推进。

## 6. 用户态为什么不能直接读取内核 tick 变量

tick 计数属于内核内部全局状态。  
用户态程序不能直接访问内核地址空间，所以必须通过 syscall 走“只读查询”路径。

## 7. get_ticks / uptime syscall 的最小语义

当前最小语义如下：

- 返回自系统启动以来累计的 PIT tick 数
- 只读，不修改内核状态
- 使用 `unsigned int` / `int` 级别返回值
- 当前暂不处理 tick 溢出

## 8. 当前实现的限制

- 只显示 ticks，不显示秒
- 暂无 RTC 真实时间
- 暂无高精度计时
- 暂不统计每个进程的 CPU 时间

## 9. 后续可扩展方向

- 基于 ticks 增加更自然的 `sleep <ticks>` shell 命令
- 统计每个进程的运行 tick
- 统计调度切换与时间片使用情况
- 后续再考虑从 ticks 推导更友好的 uptime 表示
