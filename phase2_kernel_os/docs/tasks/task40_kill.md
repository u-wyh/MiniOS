# Task40：用户态 kill 命令雏形

## 1. 当前 MiniOS 的 kill 和 Linux signal kill 有什么区别？

当前实现是教学版最小 kill：

- shell 发起 `kill <pid>`
- 内核通过 syscall 直接将目标进程标记为 `ZOMBIE`

它不是完整 signal 子系统，不支持：

- `SIGKILL/SIGTERM` 编号
- signal handler
- 进程组/会话级投递

## 2. kill 为什么不直接释放资源？

因为当前回收模型依赖 `waitpid`：

1. 进程先变成 `ZOMBIE`
2. 父进程后续 `waitpid` 时再做最终释放

如果 kill 当场直接释放，父进程回收语义会被破坏。

## 3. kill 后为什么目标进程进入 ZOMBIE？

`ZOMBIE` 是“终止但未回收”的中间态，能保留退出信息（本轮固定为 `-9`），并等待父进程统一回收，和普通 `exit` 路径保持一致。

## 4. waitpid 在 kill 后回收中负责什么？

`waitpid` 负责最终清理：

- 回收用户态镜像资源
- 清理 PCB 槽位
- 让进程从活动列表消失

所以 kill 只负责“终止请求”，不负责“最终回收”。

## 5. 为什么 init / shell 需要保护？

教学阶段最小稳定策略中，这两个进程是关键控制点：

- `init` 是根父进程
- 当前 shell 是命令发起者

本轮默认拒绝杀：

- `pid == 1`（init）
- 当前进程 pid（当前 shell）

## 6. 调度器为什么不能继续运行 ZOMBIE？

`ZOMBIE` 已结束，不应被调度执行。当前调度路径只挑 `READY`，所以标记为 `ZOMBIE` 后目标会自动被跳过。

## 7. start loop 如果实现，它和完整后台任务有什么区别？

`start <program>` 只是最小测试辅助：

- 只 `fork/exec`
- 不 `waitpid`
- 打印子进程 pid 供 `kill/wait` 验证

它不包含 job 表、前后台切换、终端控制等完整后台任务系统能力。

## 8. 后续要做真正 signal / job control 还缺什么？

- signal 编号和投递机制
- handler 注册与执行语义
- 进程组/会话模型
- 权限与安全检查
- 完整 job control（`fg/bg`、任务状态、TTY 集成）
