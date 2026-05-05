# Task41：前台 / 后台任务雏形

## 1. 前台任务和后台任务的区别是什么？

在当前 MiniOS 中，核心区别是 shell 父进程是否等待子进程：

- `run <program>`：前台执行，shell 会 `waitpid`。
- `start <program>`：后台执行，shell 不等待，立即返回提示符。

这是一种教学版最小语义，先把“执行模式”区分清楚。

## 2. `run <program>` 为什么要 `waitpid`？

`run` 代表前台命令。  
shell 只有在子进程结束后才继续读下一条命令，这样用户体验上是“命令结束 -> 再提示输入”。

## 3. `start <program>` 为什么不 `waitpid`？

`start` 代表后台启动。  
shell 只负责拉起子进程并返回控制权，后续可用 `ps/kill/wait` 继续管理该进程。

## 4. 后台任务为什么仍然需要 `wait <pid>` 回收？

后台子进程退出后不会自动释放全部资源，而是先进入 `ZOMBIE`。  
必须由父进程执行 `waitpid`（这里通过 `wait <pid>`）完成最终回收。

## 5. `kill`、`zombie`、`waitpid` 三者如何配合？

最小闭环是：

1. `kill <pid>` 请求终止目标进程；
2. 目标进入 `ZOMBIE`（保留退出信息）；
3. `wait <pid>` 调用 `waitpid`，完成最终回收。

这样可以保持“终止动作”和“资源回收”职责分离。

## 6. 当前 `start` 和 Linux shell `&` 有什么区别？

当前 `start` 是显式命令，不是 `cmd &` 语法；  
也没有 job 列表、`fg/bg`、终端控制等机制。它只是最小后台启动入口。

## 7. 当前 MiniOS 暂不支持哪些 job control 能力？

- `&` 语法
- `jobs` 命令
- `fg/bg`
- 进程组与会话
- 终端前台进程组控制

## 8. 后续要做完整 `jobs/fg/bg` 还缺什么？

- shell 任务表（pid、状态、命令）
- 前后台切换流程
- 更完整 signal 机制
- 与终端输入控制的联动
