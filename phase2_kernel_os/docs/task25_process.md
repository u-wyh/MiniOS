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

## 7. Task29 补充：process_create 与 exec 分工

Task29 继续把“创建进程”和“装载用户程序”拆开：

- `process_create(name)` 负责创建 PCB、分配 pid、记录 parent_pid。
- `process_exec(...)` 负责装载 ELF、建立用户栈、写回 `eip/esp`。
- `process_exec_file(...)` 负责把“按名字找程序”和“装载/替换镜像”连接起来。

这样后续进入 `fork + exec` 时，就不会再把“创建一个新进程”和“替换当前进程镜像”混成同一个概念。

## 8. Task30 补充：fork 与进程复制

Task30 在前面这些分层之上继续增加：

- `process_create` 更像“新建一个进程对象”
- `process_fork` 更像“复制当前进程对象和它的用户镜像”
- `process_exec` 仍然只负责“把某个程序装进进程里”

这样 MiniOS 里就开始出现三种不同语义：

- create：创建空的进程壳
- fork：复制现有进程
- exec：替换进程里的程序

这正是后续继续完善多进程语义前必须先理顺的边界。

## 9. Task31 补充：fork 不是“打印顺序测试”

Task31 进一步强调：

- fork 的正确性不应该只看“谁先打印”
- 更重要的是父子是否都从 fork 返回点继续执行
- 以及父子是否拿到了不同返回值、不同用户栈物理页

所以本轮测试重点从“有没有跑起来”推进到“返回点和资源隔离是否正确”。

## 10. Task32 补充：fork、exec、waitpid 的组合

Task32 把前面分开的几个语义真正连起来：

- `fork` 负责复制出子进程
- `exec` 负责把子进程替换成另一个程序
- `waitpid` 负责让父进程等待并回收这个子进程

这样 MiniOS 就开始具备最小的“shell 执行外部程序”骨架路径了。
