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

## 21. Task38 补充：真实 OS 通常如何传递 `argc/argv`

真实操作系统里，`execve` 通常会在新程序开始运行前，把：

- `argc`
- `argv[]`
- `envp[]`
- 参数字符串本体

一起布置到新用户栈上。

这样用户程序入口一开始就能按统一 ABI 读取自己的启动参数。

## 22. 当前 MiniOS 为什么先用 PCB 参数缓冲

因为当前阶段还没有完整的用户态启动 ABI，也不希望为了参数传递过早引入：

- 用户栈布局细节
- 参数指针对齐问题
- 更复杂的入口约定

所以 Task38 先采用教学版最小方案：

- shell 在子进程里发起 `exec_args`
- 内核先把少量参数复制到当前进程 PCB
- 新程序启动后再通过 `get_argc/get_arg` syscall 读取这些参数

这样实现简单，也能先把“参数属于新程序启动上下文”的概念打通。

## 23. `exec` 后 `pid / parent_pid` 为什么不变

因为 `exec` 的语义是“替换当前进程镜像”，不是“创建一个全新进程”。

在 Task38 中：

1. shell 先 `fork` 出子进程
2. 子进程已经有自己的 `pid`
3. 子进程再执行 `exec_args`
4. 被替换的是代码、数据和启动参数
5. 进程身份仍然是原来的那个子进程

所以 shell 仍然是父进程，而 `echo`、`hello` 仍然是 shell 的子进程。

## 24. 当前 argv 机制有哪些限制

当前实现故意保持最小化，只支持：

- 固定数量上限参数
- 固定长度上限字符串
- 普通短字符串

不支持：

- `envp`
- 引号
- 转义
- PATH 搜索
- 真正的用户栈 `argv` ABI

所以它更像“教学版启动参数暂存区”，而不是完整 Unix 进程启动模型。

## 25. 后续如何迁移到用户栈 `argc/argv` ABI

后续如果继续向真实 `execve` 靠近，通常需要补这些能力：

- 在 `exec` 时规划新用户栈顶部布局
- 把参数字符串复制到新用户栈
- 构造 `argv[]` 指针数组
- 让用户程序入口按统一 ABI 读取 `argc/argv`
- 再进一步接入 `envp`

到那一步之后，当前 PCB 暂存区就可以退场，参数会真正成为“新程序启动栈的一部分”。

## 26. Task39：用户态 ps 命令雏形

### 为什么用户态不能直接读取 PCB

PCB 是内核内部结构，里面有大量内核管理字段和地址信息。直接暴露给用户态会带来：

- 内核实现耦合
- 越界读写风险
- 未来结构演进困难

所以用户态应该看到“只读摘要”，而不是 PCB 本体。

### `process_info` 和 PCB 的区别

- PCB：内核完整进程对象，包含调度、内存、trapframe、资源回收等内部字段。
- `process_info`：用户态可见的简化视图，仅保留 `pid/ppid/state/name`。

这让 `ps` 可以观察系统状态，又不破坏内核封装边界。

### `ps` syscall 的最小语义

当前采用最小接口：

- 输入：活动进程序号 `index` + 用户缓冲区指针
- 输出：若 `index` 有对应活动进程，写入一条 `process_info` 并返回 `0`
- 若越界则返回负数，用户态据此停止遍历

这种“逐条读取”接口实现最小、可控，也足够支撑教学版 `ps`。

### `ps` 为什么不应该改变进程状态

`ps` 的职责是“观察”，不是“调度或控制”。如果查询过程会改状态，会导致：

- 观测扰动系统行为
- 和 `waitpid`、调度状态机冲突
- 调试结果不可信

所以 Task39 中 `ps` 只读遍历，不做状态迁移。

### zombie 在 `ps` 中可能如何显示

zombie 是否能看到，取决于查询时机：

- 子进程刚 `exit` 但还没被父进程 `waitpid` 回收：可能显示 `ZOMBIE`
- 已被回收：不会再出现在活动列表

因此 `ps` 展示的是“某个时刻快照”，不是历史轨迹。

### 当前 MiniOS `ps` 与 Linux `ps` 的差距

当前版本是教学最小实现，还不支持：

- 参数筛选
- 进程树视图
- CPU/内存统计
- `/proc` 数据源

但它已经建立了关键边界：用户态通过 syscall 读摘要，而不是直接碰内核 PCB。

### 后续如何从 `ps` 走向 `/proc` 或 `kill`

后续可以沿这个方向演进：

1. 扩展 `process_info` 字段（时间片、内存页数等）
2. 引入 `/proc` 风格只读视图
3. 在权限和状态校验到位后，再加 `kill` 等控制命令

Task39 的价值在于先把“观测面”搭出来，为后续进程管理功能打地基。

## 27. Task40：用户态 kill 命令雏形

### 当前 MiniOS 的 kill 和 Linux signal kill 有什么区别

当前实现是教学版“直接终止请求”：

- `kill <pid>` -> syscall -> 内核将目标标记为 `ZOMBIE`

它不是完整 signal 系统，不支持：

- `SIGKILL/SIGTERM` 编号体系
- signal handler
- 进程组/会话语义

### kill 为什么不直接释放资源

因为当前进程生命周期仍遵循：

1. 先进入 `ZOMBIE` 保存退出状态
2. 由父进程 `waitpid` 执行最终回收

如果 kill 时直接释放资源，会破坏现有 `waitpid` 路径的一致性。

### kill 后为什么目标进程进入 ZOMBIE

`ZOMBIE` 表示“已结束但尚未被父进程回收”的中间态。  
本轮 kill 仅改变状态并写入固定退出码（如 `-9`），与普通 `exit` 的回收模型保持一致。

### waitpid 在 kill 后回收中负责什么

`waitpid` 负责最终清理：

- 释放用户镜像资源
- 清理 PCB 槽位
- 让进程从活动列表消失

所以 kill 本身只做状态转换，不做最终资源释放。

### 为什么 init / shell 需要保护

教学阶段的最小稳定策略下，`init` 和当前 shell 是关键控制进程：

- 杀掉 `init` 会破坏根父进程链路
- 杀掉当前 shell 会引入命令发起者自终止的恢复复杂度

因此本轮默认拒绝：

- `pid == 1`（init）
- 当前进程 pid（当前 shell）

### 调度器为什么不能继续运行 ZOMBIE

`ZOMBIE` 进程逻辑已结束，不应再被执行。当前调度仅选择 `READY` 进程，因此目标标记为 `ZOMBIE` 后会自然被跳过。

### start loop 如果实现，它和完整后台任务有什么区别

本轮 `start` 只是测试辅助：

- 只做 `fork/exec`，不 `waitpid`
- 打印子进程 pid，方便后续 `kill/wait` 验证

它不包含完整 job control（`fg/bg`、任务列表、终端控制等）。

### 后续做真正 signal / job control 还缺什么

后续若要接近 Linux 行为，至少还需要：

- 完整 signal 编号与投递机制
- 进程组/会话模型
- 权限校验
- 后台任务管理与终端控制

## 28. Task41：前台 / 后台任务雏形

### 前台任务和后台任务的区别是什么

教学版最小区别只有一条：父进程 shell 是否等待子进程结束。

- 前台：shell `fork/exec` 后立即 `waitpid`，命令结束前不返回提示符。
- 后台：shell `fork/exec` 后不 `waitpid`，立刻返回提示符继续接收命令。

### `run <program>` 为什么要 waitpid

`run` 代表前台执行语义。  
shell 等待子进程结束后再回到命令循环，可以保证“命令执行完成 -> 再显示提示符”的顺序稳定。

### `start <program>` 为什么不 waitpid

`start` 代表后台启动语义。  
它的目标是“先把程序拉起来，再把控制权还给 shell”，方便继续输入 `ps/kill/wait` 管理该子进程。

### 后台任务为什么仍然需要 `wait <pid>` 回收

后台子进程结束后会进入 `ZOMBIE`（或等价退出态）。  
如果父进程不执行 `waitpid`，该 PCB 不会被最终回收，可能持续占用进程槽位。

### kill、zombie、waitpid 三者如何配合

最小链路是：

1. `kill <pid>` 让目标进程进入 `ZOMBIE`
2. `ps` 可观察该状态变化
3. `wait <pid>` 调用 `waitpid` 完成最终回收

这样保持了“终止”和“回收”职责分离，和已有 `exit/waitpid` 模型一致。

### 当前 `start` 与 Linux `&` 的区别

当前 `start` 是显式命令，不是语法后缀。  
它不包含 job 表、前后台切换、TTY 控制等完整能力，只是教学版最小后台启动入口。

### 当前 MiniOS 暂不支持哪些 job control 能力

- `&` 语法
- `jobs`
- `fg/bg`
- 进程组/会话
- 终端前台进程组控制

### 后续要做 `jobs/fg/bg` 还缺什么

后续至少还需要：

- shell 内部任务表（记录 pid、状态、命令行）
- 前后台切换协议
- 终端控制与输入归属管理
- 更完整的 signal 与状态同步机制

## 29. Task42：孤儿进程 reparent 到 init

### 什么是孤儿进程

当子进程还存活，但它的父进程先退出时，这个子进程就是孤儿进程。

### 为什么 shell 退出后后台任务会变成孤儿进程

`start <program>` 启动的后台任务本质上是 shell 子进程。  
如果 shell 先退出，后台任务还在运行或尚未回收，就会失去原父进程。

### 为什么需要把孤儿进程交给 init

当前 MiniOS 的最小模型里，init 是根用户进程。  
把孤儿进程交给 init，可以保证后续仍有父进程语义用于 `waitpid` 回收。

### reparent 具体修改了什么

在父进程退出路径中扫描进程表，把 `parent_pid == exiting_pid` 的有效子进程改成 `parent_pid = init_pid`。

### reparent 为什么不等于 kill

reparent 不会结束子进程，也不会写退出码。  
它只改变父子关系，子进程状态保持原样（READY/RUNNING/ZOMBIE）。

### reparent 为什么不释放资源

资源释放仍属于退出回收链路：子进程退出后由父进程 `waitpid` 回收。  
reparent 只负责“把父进程关系改正确”。

### init 在当前 MiniOS 中承担什么角色

当前 init 负责：

- 系统首个用户进程
- shell 的父进程
- 孤儿进程接管目标（最小 reaper 角色）

### 当前实现和真实 Linux reparent 还有哪些差距

当前仍是教学版最小实现，不包含：

- 进程组/session
- 终端控制
- 完整 signal 与 job control
- 完整自动 reaper 策略

## 30. Task43：init reaper 循环雏形

### reparent 和 reap 的区别是什么

- reparent：只改父子关系（`parent_pid`），不结束进程、不释放资源。
- reap：回收已退出子进程（`ZOMBIE`）的资源并释放 PCB 槽位。

### 为什么 ZOMBIE 必须由父进程 wait 回收

`ZOMBIE` 代表“已退出但未回收”。  
只有父进程执行 wait 路径时，内核才安全释放其用户镜像和 PCB。

### init 为什么适合作为孤儿进程 reaper

在当前 MiniOS 里，init 是根用户进程。  
孤儿进程 reparent 给 init 后，init 天然就是最小统一回收者。

### wait_any 的最小语义是什么

`wait_any` 当前采用教学版非阻塞语义：

- 返回 `>0`：成功回收一个子进程并返回其 pid
- 返回 `0`：当前没有可回收的 zombie 子进程
- 返回 `<0`：错误

### 为什么本轮采用非阻塞 wait_any

当前阶段不引入复杂 wait 队列与阻塞调度细节。  
非阻塞轮询语义足够打通“init 周期性回收”闭环，同时保持系统结构简单。

### init reaper 为什么不能误回收非 init 子进程

`wait_any` 只扫描 `parent_pid == current_process->pid` 且 `state == ZOMBIE` 的进程。  
这样不会误回收仍属于 shell 或其他父进程的活动子进程。

### 当前实现和真实 Linux init / wait / SIGCHLD 的差距

当前仍是教学版最小实现，不包含：

- `SIGCHLD` 通知机制
- 完整 `wait(-1)` 与阻塞语义
- 完整 init 服务管理
- 进程组/session 与终端控制

### 后续如果做完整 wait(-1) 需要补什么

- 可阻塞 wait 队列
- 子进程状态变化通知机制
- 更完整的父子同步与并发保护

## 31. Task44：用户态 yield / sleep 系统调用雏形

### yield 和 sleep 的区别是什么

- `yield`：主动让出 CPU，不设置长期等待条件。
- `sleep(ticks)`：把进程置为 `SLEEPING`，直到 tick 到期再回到可调度状态。

### 为什么 busy wait 不适合 init reaper 和后台任务

busy wait 会持续占用 CPU，导致系统空转发热、交互延迟变大。  
引入 `sleep` 后，空闲轮询路径可让出 CPU 并在到期后唤醒。

### PIT tick 如何驱动 sleep 唤醒

PIT IRQ0 每次 tick 递增系统节拍后，遍历进程表：  
当 `now_tick >= wakeup_tick` 时，把该进程从 `SLEEPING` 改为 `READY`。

### SLEEPING 状态和 READY / RUNNING / ZOMBIE 有什么区别

- `READY`：可运行，等待被调度
- `RUNNING`：当前正在执行
- `SLEEPING`：等待时间到期，不应被调度
- `ZOMBIE`：已退出，等待父进程回收

### 调度器为什么要跳过 SLEEPING

`SLEEPING` 的语义就是“到期前不运行”。  
若不跳过，会破坏 sleep 的时间等待语义。

### sleep 到期后为什么只是变回 READY

唤醒只表示“可以参与调度”，不表示“立即抢占运行”。  
最小模型中由后续调度点决定何时真正执行。

### 当前 MiniOS sleep 和 Linux sleep/nanosleep 的差距

当前是教学版最小实现：

- tick 粒度
- 无高精度定时器
- 无 signal 中断睡眠
- 无复杂阻塞队列

### 后续要做阻塞队列 / 定时器队列还缺什么

- 更完整的可运行/阻塞队列结构
- 高精度计时与定时器管理

## 32. Task45：用户态 uptime / ticks 命令雏形

### PIT tick 是什么

PIT tick 是可编程定时器 IRQ0 周期中断驱动下递增的系统节拍计数。  
它代表“系统自启动以来经历了多少个定时中断”。

### tick 和真实时间有什么区别

tick 只是内核内部节拍，不是现实世界的日期时间。  
当前 MiniOS 只知道“过了多少个定时器中断”，并不知道年/月/日/时区。

### sleep(ticks) 为什么依赖 tick

当前 `sleep(ticks)` 的语义是“等到未来某个 tick 再恢复”。  
所以必须依赖 PIT 持续递增的 tick 计数来判断是否到期。

### 用户态为什么不能直接读取内核 tick 变量

tick 变量属于内核地址空间内部状态。  
用户态程序不能直接读写它，否则会破坏内核封装与保护边界。

### get_ticks / uptime syscall 的最小语义是什么

最小语义就是只读返回“当前累计 tick 数”：

- 不修改任何内核状态
- 不做真实时间格式化
- 仅作为用户态观察系统节拍的接口

### 当前 uptime 为什么只显示 ticks

因为当前阶段还没有 RTC、日期换算、秒级格式化和时区支持。  
直接显示 ticks 最符合教学版最小实现，也最能反映 PIT/sleep 的底层关系。

### 后续如何从 ticks 扩展到 sleep 命令、进程运行时间统计

有了稳定的 tick 查询接口后，后续可以继续扩展：

- 用户态 `sleep <ticks>` 命令
- 每个进程的累计运行 tick 统计
- 调度器时间片与切换次数统计
- 更接近 uptime/top 的系统观察命令
- 与事件/信号机制联动的唤醒策略

## 33. Task46：用户态 sleep 命令雏形

### sleep 命令和 sleep syscall 的关系是什么

当前 `sleep <ticks>` 只是用户态 shell 对已有 `sleep(ticks)` syscall 的最小封装。  
shell 自己负责解析命令参数，真正把进程改成 `SLEEPING`、记录唤醒 tick、等待 PIT 唤醒的工作仍在内核里完成。

### sleep <ticks> 为什么使用 tick 作为单位

因为当前内核已有稳定的 PIT tick 计数和唤醒逻辑。  
直接使用 tick 可以复用已有 `SYS_SLEEP` 与 `SYS_GET_TICKS`，避免在本阶段额外引入秒级换算和 RTC 逻辑。

### shell 调用 sleep 后，为什么 shell 自己会暂停

因为执行命令的主体就是 shell 进程本身。  
当 shell 调用 `sleep(ticks)` 时，睡眠的是“当前 shell 进程”，不是某个抽象命令对象。

### shell 睡眠期间为什么不能继续处理命令

因为 shell 已经进入 `SLEEPING` 状态，此时它不会继续运行 `read_line` 和命令分发逻辑。  
只有 PIT tick 到期、内核把 shell 改回 `READY`，并再次调度到 shell 时，命令循环才会继续。

### PIT tick 如何唤醒 shell

Task44 已经实现了：

- shell 调用 `SYS_SLEEP`
- 内核记录 `wakeup_tick = now + ticks`
- 进程状态改成 `SLEEPING`
- PIT 每次 tick 调用唤醒检查
- 到期后把 shell 从 `SLEEPING` 改回 `READY`

之后调度器再次选中 shell，sleep syscall 返回，shell 回到主循环重新打印提示符。

补充说明：当前教学版系统如果暂时只有 `init + shell` 两个活动进程，没有其他 READY 进程可切换，
shell 命令层会回退到基于 `get_ticks` 的最小等待，优先保证 `sleep <ticks>` 的可见语义稳定。

### uptime 如何验证 sleep 的效果

最直接的验证方式是：

1. 执行 `uptime`
2. 执行 `sleep 100`
3. 再执行 `uptime`

如果第二次的 tick 数明显大于第一次，通常至少增加了约 100 tick，就说明 `sleep <ticks>` 已按预期等待了一段节拍时间。

### 当前 sleep 命令和 Linux sleep 有什么差距

当前实现仍是教学版最小模型：

- 单位是 tick，不是秒
- 不支持 `sleep 1s` / `sleep 1m`
- 不支持高精度定时器
- 不支持信号中断 sleep
- 不支持复杂阻塞队列和超时管理

所以它更接近“把 shell 进程挂起若干定时节拍”，而不是 Linux 的完整时间接口。

### 后续要支持秒级 sleep / 可中断 sleep 还缺什么

后续如果要更接近真实系统，还需要继续补：

- tick 到秒/毫秒的换算接口
- 更清晰的用户态时间 API
- 可中断 sleep 语义
- 更完整的阻塞/唤醒队列
- 更高精度的定时器基础设施

## 34. Task47：进程创建时间 / 存活时间统计雏形

### create_tick 表示什么

`create_tick` 记录的是“这个进程对象被创建出来时，系统已经走到了第几个 PIT tick”。  
它描述的是进程生命周期起点，不是 CPU 实际执行了多久。

### age_ticks 如何计算

当前实现采用最小公式：

```text
age_ticks = current_ticks - create_tick
```

其中 `current_ticks` 直接复用已有 `pit_get_ticks()`。  
如果 `create_tick` 还是 `0`，当前就返回 `0`，避免把未初始化槽位当成真实进程时间。

### 为什么 AGE 不是 CPU 运行时间

因为它只统计“进程已经存在了多久”：

- READY 时 AGE 继续增长
- SLEEPING 时 AGE 继续增长
- BLOCKED 时 AGE 继续增长
- 就算进程暂时没拿到 CPU，AGE 也照样增长

所以 AGE 更接近“存活时长”，而不是“占用处理器时长”。

### fork 为什么要设置新的 create_tick

`fork` 会产生一个新的子进程：

- 子进程有新的 pid
- 子进程有新的 PCB
- 子进程应该有自己的生命周期起点

因此子进程不能继承父进程的 `create_tick`，而应该在 `fork` 时重新记录当前 tick。

### exec 为什么不重置 create_tick

`exec` 不是创建新进程，而是替换“同一个进程”的用户态镜像：

- pid 不变
- 父子关系不变
- 只是程序内容变了

如果在 `exec` 时重置 `create_tick`，就会把“同一个进程”误看成“新进程”，破坏生命周期语义。

### PCB 回收时为什么要清理 create_tick

当前 MiniOS 会复用 PCB 槽位。  
如果旧进程退出后不把 `create_tick` 清零，新进程复用这个槽位时，`ps` 可能会读到旧值，出现明显错误的 AGE。

所以本轮把清理逻辑放进统一的 `process_clear_slot()`，避免多处复制和漏清理。

### ps 显示 AGE 有什么调试意义

它能帮助我们快速判断：

- 哪些进程是系统长期存在的基础进程（如 `init`、`shell`）
- 哪些进程是刚刚创建出来的短命进程（如 `hello`、`echo`）
- 后台进程是否真的一直活着（如 `loop`、`sleep_test`）
- `sleep` 前后进程是否确实经历了一段时间

这对于教学操作系统非常有价值，因为它给了我们一个简单、直观、低侵入的生命周期观测口。

### 后续如何扩展到运行 tick / 调度次数统计

Task47 只是先把“生命周期起点”记下来。  
后续如果要继续扩展，可以在此基础上增加：

- `run_ticks`：只在真正运行时累加
- `kernel_ticks` / `user_ticks`：区分执行场景
- `switch_count`：统计被调度多少次
- 更丰富的 `process_info` 字段

也就是说，本轮先解决“什么时候出生”，后面再逐步解决“跑了多久、切了几次、CPU 用了多少”。

## 35. Task48：进程调度次数统计雏形

### AGE 和 RUNS 的区别是什么

这两个字段描述的是两种完全不同的观察维度：

- `AGE`：进程从创建到现在存在了多久
- `RUNS`：进程被调度器选中运行过多少次

所以一个进程可能：

- `AGE` 很大，但 `RUNS` 不多
- `AGE` 中等，但 `RUNS` 增长很快

它们不能互相替代。

### schedule_count 表示什么

`schedule_count` 表示：

- 每当内核真正决定“接下来由这个进程运行”
- 就给这个进程加一次计数

当前它只统计“被选中运行的次数”，不统计每次到底跑了多久。

### 为什么 RUNS 不是 CPU 占用率

因为 `RUNS` 只是一种“次数统计”：

- 被调度一次算 1
- 被调度十次算 10

但每次运行的时长并没有在本轮统计。  
所以 `RUNS` 不能直接换算成百分比 CPU 占用率。

### 为什么 RUNS 不是精确运行时间

即使两个进程的 `RUNS` 一样，它们的实际运行时间也可能不同：

- 一个进程可能每次很快就阻塞
- 另一个进程可能每次都跑满时间片

因此 `RUNS` 只能说明“调度器给过它多少次机会”，不能说明“它总共跑了多少 tick”。

### 调度器应该在什么时候递增 RUNS

正确的位置是：

- 已经确定某个进程即将进入 `RUNNING`
- 即将恢复它的用户态现场或直接进入它的入口

不能放在：

- 候选进程遍历阶段
- `ps` 查询阶段
- 普通状态打印阶段

这样才能保证统计语义稳定。

### 为什么 SLEEPING 进程不应持续增加 RUNS

因为 `SLEEPING` 进程不应该被调度器选中运行。  
它应该先等待：

1. 到达唤醒 tick
2. 状态从 `SLEEPING` 变回 `READY`
3. 之后再次被调度器选中

所以它在睡眠期间，`AGE` 可以继续增长，但 `RUNS` 不应持续快速增加。

### ps 显示 RUNS 有什么调试意义

它能帮助我们判断：

- 哪些进程只是存在，但很少真正被调度
- 哪些后台进程一直在反复获得 CPU
- `sleep` 或 `BLOCKED` 是否真的让进程停止参与调度
- 前台/后台命令的调度趋势是否大致合理

这对教学操作系统很有帮助，因为它提供了一个比“只看状态”更进一步的调度观察窗口。

### 后续如何扩展到运行 tick / CPU 时间统计

Task48 只是先记录“被调度了多少次”。  
后续如果继续增强，可以逐步增加：

- `run_ticks`：累计真正运行的 tick
- 用户态 / 内核态时间拆分
- 上下文切换次数和原因
- 更接近 `top` 的实时观测信息

也就是说，本轮先解决“被选中过多少次”，后面再解决“每次到底跑了多久”。

## 36. Task52：用户程序退出状态 / wait 语义整理

### 当前进程生命周期

当前 MiniOS Phase2 采用教学版最小生命周期：

```text
create / exec
    -> READY
        -> RUNNING
            -> exit(status)
                -> ZOMBIE
                    -> wait / reap
                        -> UNUSED
```

这里的重点不是完整复刻 Linux，而是保证用户程序退出后有一个清楚、可观察、可回收的闭环。

### exit(status) 做了什么

用户程序通过 `SYS_EXIT` 退出时，内核会：

- 把退出码保存到 PCB 的 `exit_status`
- 把当前进程状态改成 `PROCESS_ZOMBIE`
- 清空当前运行进程指针，避免该进程继续被当作 RUNNING 执行
- 如果父进程正在 `waitpid` 阻塞等待它，则恢复父进程并回收子进程

因此，`ZOMBIE` 不是“还在运行”，而是“已经退出，但还保留少量信息等父进程读取”。

### wait / waitpid / wait_any 的关系

当前保留三种教学版路径：

- `waitpid(pid)`：等待或回收指定子进程
- `wait_any()`：非阻塞回收当前进程名下任意一个 ZOMBIE 子进程
- shell 的 `wait [pid]`：无 pid 时走 `wait_any()`，有 pid 时走 `waitpid(pid)`

wait 只按 `parent_pid` 回收当前进程的子进程。这样 shell 不会误回收 init 的子进程，也不会回收无关后台任务。

### run / start / wait 的关系

- `run <program>`：前台执行，shell 创建子进程后等待它退出
- `start <program>`：后台执行，shell 创建子进程后立即返回
- `wait`：手动回收已经退出的后台子进程
- `wait <pid>`：针对指定子进程等待或回收

这让前台和后台路径的职责更清楚：前台由 shell 自动等，后台由用户或 init/reaper 后续回收。

### ps 中 EXIT 的意义

`ps` 显示的 `EXIT` 列来自 PCB 的 `exit_status`。

- 正常运行的进程通常显示 `0`
- 正常退出的 `loop_exit` 通常显示 `0`
- 被 `kill` 标记退出的进程会显示类似 `-9`

它不是 shell 命令返回码系统，只是当前教学版进程表里保存的退出状态。

### 当前限制

- 暂不实现完整 Linux `waitpid`
- 暂不支持信号
- 暂不支持进程组、session 和 TTY 控制
- `exit_status` 目前主要通过 `ps` 观察，`wait` 返回值仍以 pid 为主

## 37. Task53：进程父子关系 / reparent 语义整理

### 当前进程树语义

MiniOS Phase2 当前采用最小教学版进程树：

```text
init
    -> shell
        -> hello / echo / loop / loop_exit / sleep_test
```

这里的父子关系不是完整 Linux 进程树实现，而是通过 PCB 里的 `parent_pid` 字段表达“谁创建了谁、谁负责回收谁”。

### parent_pid 约定

- `PROCESS_ROOT_PARENT_PID = 0`
- init 是根进程，因此 init 的 `PPID` 显示为 `0`
- shell 由 init 启动，因此 shell 的 `PPID` 指向 init
- shell 通过 `run/start` 创建的用户程序，`PPID` 指向 shell

这样 `ps` 里的 `PID / PPID` 可以直接帮助观察进程层次。

### reparent to init

父进程退出前，内核会扫描进程表：

```text
parent exits
    -> find children whose parent_pid == parent.pid
        -> child.parent_pid = init_pid
```

这一步只修改 `parent_pid`，不改变子进程的运行状态，也不直接释放仍在运行的子进程资源。
它的目的只是避免子进程继续指向一个已经退出或即将释放的父进程。

### wait / reaper 规则

当前规则保持简单：

- 普通进程 `wait` 只回收 `parent_pid == current.pid` 的 `ZOMBIE` 子进程
- `waitpid(pid)` 也必须确认目标是当前进程的子进程
- `wait_any()` 只扫描当前进程名下的已退出子进程
- init/reaper 只兜底回收已经挂到 init 名下、且没有父进程正在等待的孤儿 `ZOMBIE`

这样 shell 不会误回收 init 的子进程，也不会误删无关进程。

### ps 显示

`ps` 中的 `PPID` 表示当前记录的父进程 pid：

- `init` 的 `PPID` 为 `0`
- `shell` 的 `PPID` 为 init 的 pid
- `start loop` 后，`loop` 的 `PPID` 应指向 shell
- 父进程退出后，孤儿子进程的 `PPID` 会变为 init 的 pid

### 当前限制

- 暂不实现完整 Linux `waitpid`
- 暂不支持信号系统
- 暂不支持进程组、session 和 TTY 控制
- 暂不维护复杂子链表，只用 `parent_pid` 扫描进程表
- 后续可以扩展更完整的进程树、权限检查和 waitpid 选项

## 38. Task54：kill syscall / shell kill 命令整理

### 当前 kill 语义

MiniOS 当前的 `kill(pid)` 是教学版进程终止接口，不是完整 Unix/Linux 信号系统。

它的最小语义是：

```text
kill(pid)
    -> 找到目标进程
        -> 写入 PROCESS_KILL_EXIT_STATUS
            -> 标记为 PROCESS_ZOMBIE
                -> 等待 wait / reaper 回收
```

这里的 `PROCESS_KILL_EXIT_STATUS` 只表示“该进程被 kill 终止”，不表示 `SIGKILL`，也不支持信号编号。

### kill 后为什么进入 ZOMBIE

kill 不直接释放 PCB 和用户页资源，而是复用 Task52 整理出来的生命周期：

```text
running / ready / sleeping
    -> zombie
        -> wait / reap
            -> unused
```

这样父进程仍然有机会通过 `wait` 或 `waitpid` 观察到子进程退出，并且资源释放只走一套回收路径。

### scheduler 如何处理被 kill 的进程

被 kill 的进程状态会变成 `PROCESS_ZOMBIE`。调度器只选择 `PROCESS_READY` 的进程，因此目标不会继续被调度运行。

### wait / reaper 如何回收

- shell 启动的后台任务被 kill 后，shell 可以执行 `wait` 或 `wait <pid>` 回收
- 如果目标已经 reparent 给 init，则 init/reaper 负责兜底回收
- 已回收后的进程槽会回到 `PROCESS_UNUSED`，`ps` 不应继续显示脏数据

### 安全限制

- 不允许 kill init，避免系统失去根进程
- 不允许当前 shell 直接 kill 自己，避免本轮引入自杀恢复路径
- 不支持 `kill -9`
- 不支持进程组 kill
- 不支持权限模型
