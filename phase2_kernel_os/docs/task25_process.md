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

## 11. Task33 补充：第一个用户态 init

Task33 再往前推进一步，把“谁来驱动这条进程路径”从内核移动到用户态：

- 内核启动后先创建固定 `init` 用户进程
- `init` 成为第一个用户进程，因此自然是 `pid 1`
- `init` 在用户态执行 `fork`
- 子进程在用户态执行 `exec`
- `init` 再通过 `waitpid` 回收这个子进程

这样做的教学意义在于：

- 内核只负责把第一个用户进程拉起来
- 真正的用户程序管理逻辑开始由用户态进程承担
- 后续用户态 shell 可以看作“更复杂的 init”

当前 MiniOS 的 `init` 仍然是固定逻辑，不支持：

- 命令解析
- `argv/envp`
- 路径字符串 `exec`
- 脚本化 init 流程

但它已经清楚展示了一个结构：系统启动后，应该逐步由用户态进程而不是内核测试代码去管理后续用户程序。

## 12. Task34 补充：从 init 走向用户态 shell

Task34 再往前推进一层，把当前固定用户态层次变成：

- `init`
- `shell`
- `user program`

也就是说，现在不再是 `init` 直接去运行普通测试程序，而是：

1. `init` 先 `fork`
2. 子进程 `exec` 成 `shell`
3. `shell` 再 `fork`
4. `shell` 的子进程 `exec` 成 `hello`
5. `shell` 回收 `hello`
6. `init` 再回收 `shell`

这样 MiniOS 里开始出现更接近真实系统的用户态层次：

- `init` 负责启动和管理 `shell`
- `shell` 负责启动和管理普通用户程序

当前 `shell` 仍然是固定脚本式，不支持：

- 交互输入
- 命令解析
- `argv/envp`
- PATH 搜索

但它已经足够展示一个关键点：

shell 本质上也只是普通用户进程，只不过它承担了“启动别的用户程序”的管理角色。

## 13. Task36 补充：交互式 shell 为什么仍然要走 fork / exec / waitpid

Task36 让 `shell` 从固定脚本式变成了最小交互式，但它执行 `hello` 命令时，核心进程语义并没有变。

原因是：

- 如果 `shell` 直接 `exec` 成 `hello`，那 `shell` 自己就消失了
- 命令执行完后，也就没人继续显示提示符、接受下一条命令

所以当前最小正确路径仍然是：

1. `shell` 先 `fork`
2. 子进程 `exec` 成 `hello`
3. 父进程 `waitpid(child_pid)`
4. `hello` 退出后，父进程 `shell` 继续回到交互循环

这就是为什么即使只有一个 `hello` 命令，也必须坚持走 `fork/exec/waitpid`。

## 14. init、shell、hello 三者的父子关系是什么

Task36 完成后，最小用户态层次是：

`init -> shell -> hello`

形成方式是：

- 内核先启动 `init`
- `init` `fork` 出子进程，再让子进程 `exec` 成 `shell`
- `shell` 收到 `hello` 命令后，再 `fork` 出自己的子进程
- 这个子进程再 `exec` 成 `hello`

因此：

- `shell.parent_pid == init.pid`
- `hello.parent_pid == shell.pid`

这也解释了为什么 `init` 只负责回收 `shell`，而 `shell` 自己负责回收 `hello`。

## 15. 当前 shell 和 Phase1 Shell 的相似点与差异

相似点：

- 都有提示符
- 都要把一行输入解释成一个命令
- 都需要根据命令名做分发

差异点：

- Phase1 shell 主要是内核里的命令解释器
- Task36 的 shell 是普通用户进程
- Task36 shell 通过 syscall 读取输入、启动程序和等待子进程

所以 Task36 更接近真实 OS 语义，而不是单纯的命令字符串演示。

## 16. 当前最小交互式 shell 还缺什么

本轮故意只支持固定字符串匹配：

- `help`
- `hello`
- `exit`

仍然没有：

- 参数解析
- PATH 搜索
- `argv/envp`
- 管道
- 重定向
- 文件描述符表

这说明当前 shell 已经具备“交互闭环”，但还不是完整 Unix shell。

## 17. Task37 补充：为什么 `echo` 是内建命令

`echo` 的作用只是把 shell 自己已经拿到的参数重新输出出来。

它不需要：

- 加载新的用户程序
- 替换当前进程镜像
- 产生新的子进程

所以最自然的做法就是让 shell 自己直接处理它。

这也是为什么当前 `echo` 属于“内建命令”，而不是通过 `fork/exec` 去启动一个外部 `echo` 程序。

## 18. Task37 补充：为什么 `run` 仍然必须走 `fork / exec / waitpid`

和 Task36 的 `hello` 命令一样，`run` 的职责是“让 shell 启动另一个用户程序”，而不是让 shell 自己消失。

因此当前最小正确路径仍然是：

1. shell `fork`
2. 子进程 `exec` 成目标程序
3. 父进程 `waitpid`

这样程序退出后，shell 才能继续回到提示符循环。

所以：

- `echo` 是内建命令
- `run` 是外部程序启动入口

两者的本质职责并不相同。

## 19. 当前 `run <program>` 为什么只支持固定内置程序

因为当前 MiniOS 的 `exec` 还不是“按路径字符串装载任意文件”的完整语义。

现阶段内核里仍然是：

- shell 先把程序名识别出来
- 再把它翻译成固定 `program_id`
- 内核根据这个 id 找到对应内置 ELF

所以本轮 `run` 只是一个最小教学版外部程序入口，而不是完整文件系统命令执行器。

## 20. 后续要支持真正的 `exec argv/envp` 还需要补什么

如果后续继续推进，至少还需要：

- 路径字符串到程序文件的查找
- `exec(path, argv, envp)` 形式的参数入口
- 用户栈上的参数布局
- `argv/envp` 从父进程传给新程序
- 更完整的 stdin/stdout 与文件系统配合

Task37 先完成的是更基础的一步：

它让 shell 已经具备“先把命令拆成 token，再决定是自己执行，还是通过 `fork/exec/waitpid` 启动外部程序”的最小结构。
