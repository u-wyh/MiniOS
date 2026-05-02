# Task25：进程模型（Process）+ PCB

## 1. 什么是进程

进程是“正在运行的程序实例”。

与“程序文件”不同，进程不仅包含代码，还包含运行时状态（寄存器、栈、状态位、标识符等）。

## 2. PCB 作用

PCB（Process Control Block）用于描述并管理一个进程。

本任务最小 PCB 字段：

- `pid`：进程唯一标识
- `state`：进程状态（READY/RUNNING/EXIT）
- `esp`：栈顶指针
- `eip`：入口指令地址

通过 PCB，内核能知道“这个进程是谁、在什么状态、从哪儿继续执行”。

## 3. 进程 vs 线程

- 进程：资源与执行上下文的主体（地址空间、句柄等）
- 线程：进程内部的执行流

本任务只实现最小进程模型，不拆分线程层级。

## 4. 调度关系

本轮目标是把执行主体从 task 语义过渡到 process 语义：

- `exec(name)` 不再直接加载运行
- 改为 `process_create(name)` 创建 PCB
- 再由 `process_run(proc)` 进入用户态

调度器本轮保持最小占位，不实现复杂时间片与多进程切换，但执行控制关系已变为：

`shell -> exec -> process_create -> process_run -> user mode`

## 5. shell 集成

新增 `ps` 命令，用于输出进程列表：

- PID
- STATE
- NAME

这样可以观察进程创建和状态变化。

## 6. Task28 补充：进程资源回收

在 Task28 中，MiniOS 把回收时机固定在 `wait/waitpid`：

- `exit` 只把进程标记为 `ZOMBIE`，保留退出记录。
- `wait/waitpid` 再释放用户栈页和 ELF 映射页。
- 最后把 PCB 状态重置为 `UNUSED` 供后续复用。

这样可以保证父进程先读取退出信息，再做资源释放，避免“退出即销毁”导致状态丢失。
