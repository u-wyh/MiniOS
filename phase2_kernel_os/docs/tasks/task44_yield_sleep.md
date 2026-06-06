# Task44：用户态 yield / sleep 系统调用雏形

## 1. yield 和 sleep 的区别是什么？

- `yield`：当前进程主动让出 CPU，但不进入长期等待状态。
- `sleep(ticks)`：当前进程进入 `SLEEPING`，直到 tick 到期再恢复为 `READY`。

## 2. 为什么 busy wait 不适合 init reaper 和后台任务？

busy wait 会持续占用 CPU，导致系统空转和交互迟缓。  
使用 `yield/sleep` 能把等待期让给其他 READY 进程。

## 3. PIT tick 如何驱动 sleep 唤醒？

PIT 每次中断都会更新 tick，随后检查所有 `SLEEPING` 进程。  
到达 `wakeup_tick` 后进程状态改回 `READY`。

## 4. SLEEPING 和 READY/RUNNING/ZOMBIE 有什么区别？

- `READY`：可运行，等待调度
- `RUNNING`：正在运行
- `SLEEPING`：时间未到，不可运行
- `ZOMBIE`：已退出，等待父进程回收

## 5. 调度器为什么要跳过 SLEEPING？

因为 sleep 的语义就是“到期前不运行”。  
只有被唤醒后（回到 READY）才允许被调度。

## 6. sleep 到期后为什么只是变回 READY？

唤醒只代表“具备运行资格”，不是“立刻抢占 CPU”。  
最小实现中继续由后续调度点选择运行时机。

## 7. 当前实现和 Linux sleep/nanosleep 的差距

- tick 粒度，不是高精度时间
- 无 signal 打断/恢复
- 无复杂阻塞队列
- 无 select/poll 类等待复用

## 8. 后续若要做完整定时阻塞还缺什么？

- 完整阻塞队列与唤醒队列
- 更精细定时器管理
- 与 signal/事件系统联动
