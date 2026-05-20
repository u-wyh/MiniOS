# MiniOS Phase2 用户程序表

## 1. 当前定位

当前 MiniOS Phase2 还没有真实文件系统。

因此：

```text
run hello
```

并不是像 Linux 一样去磁盘路径查找程序，而是走教学版最小链路：

```text
program name -> program_id -> 内置 ELF/blob -> exec -> process
```

Task50 的目标，就是把这条链路里的程序编号、程序名和内置镜像管理统一起来。

## 2. program_id 表

- `PROGRAM_EXEC_CHILD = 1`
- `PROGRAM_SHELL = 2`
- `PROGRAM_HELLO = 3`
- `PROGRAM_ECHO = 4`
- `PROGRAM_LOOP = 5`
- `PROGRAM_SLEEP_TEST = 6`
- `PROGRAM_INIT = 7`
- `PROGRAM_LOOP_EXIT = 8`
- `PROGRAM_INFO = 9`
- `PROGRAM_FORK = 10`
- `PROGRAM_FORKEXEC = 11`
- `PROGRAM_CAT = 12`
- `PROGRAM_LS = 13`
- `PROGRAM_STAT = 14`

其中：

- `PROGRAM_INVALID = 0` 表示非法编号
- `PROGRAM_COUNT` 只用于边界检查

## 3. 程序名映射

- `init -> PROGRAM_INIT`
- `shell -> PROGRAM_SHELL`
- `hello -> PROGRAM_HELLO`
- `echo -> PROGRAM_ECHO`
- `loop -> PROGRAM_LOOP`
- `loop_exit -> PROGRAM_LOOP_EXIT`
- `sleep_test -> PROGRAM_SLEEP_TEST`
- `execchild -> PROGRAM_EXEC_CHILD`
- `info -> PROGRAM_INFO`
- `fork -> PROGRAM_FORK`
- `forkexec -> PROGRAM_FORKEXEC`
- `cat -> PROGRAM_CAT`
- `ls -> PROGRAM_LS`
- `stat -> PROGRAM_STAT`

其中 shell 默认直接暴露给用户的程序主要是：

- `hello`
- `echo`
- `ls`
- `cat`
- `stat`
- `loop`
- `loop_exit`
- `sleep_test`

兼容别名：

- `sleeptest -> sleep_test`
- `loopexit -> loop_exit`

## 4. exec 语义

当前教学版 `exec` 的最小语义是：

1. shell 先把程序名解析成 `program_id`
2. `SYS_EXEC_ARGS` 把 `program_id` 和最小 `argv` 交给内核
3. 内核通过统一用户程序表查到目标描述符
4. 描述符再关联到内置 ELF/blob
5. 内核把目标镜像装入当前进程地址空间

## 5. shell run/start 语义

- `run <program>`：前台执行，shell `fork` 子进程，子进程 `exec`，父进程 `waitpid`
- `start <program>`：后台执行，shell `fork` 子进程并 `exec`，父进程不等待

## 6. 用户程序参数传递

当前 MiniOS 还没有完整 `execve(path, argv, envp)` 和用户栈 `argc/argv` ABI，因此先采用教学版最小链路：

```text
shell token -> program_id -> SYS_EXEC_ARGS -> PCB 暂存 argv -> 用户程序通过 get_argc/get_arg 读取
```

例如：

```text
run echo hello minios
```

会被解释为：

- program name：`echo`
- argv[0]：`echo`
- argv[1]：`hello`
- argv[2]：`minios`

也就是说，当前实现保留了最小 Unix 风格语义：程序名会作为 `argv[0]` 传给新程序。

## 7. 参数限制

- 最大参数数量：`8`
- 单个参数最大长度：`31` 个可见字符，外加结尾 `'\0'`
- 参数过多：shell 会直接报错 `Too many args`，不会继续 `fork/exec`
- 参数过长：shell 会直接报错 `Arg too long`，内核 `process_copy_user_args()` 也会做兜底校验

## 8. echo 验证方式

- `run echo`：验证“无附加参数”路径，程序应安全输出空行并退出
- `run echo hello`：验证单参数传递
- `run echo hello minios phase2`：验证多参数与顺序保持

## 9. 退出与等待语义

当前用户程序退出采用教学版最小闭环：

```text
run/start -> running -> exit(status) -> ZOMBIE -> wait/reaper -> free slot
```

当前行为是：

- `run <program>`：前台启动，shell 会等待指定子进程退出后再返回提示符
- `start <program>`：后台启动，shell 不等待，立即返回提示符
- `wait`：非阻塞回收任意一个已经退出的子进程
- `wait <pid>`：针对指定子进程执行当前最小 `waitpid` 语义

为了便于观察退出路径，`ps` 现在会额外显示 `exit_status` 列。

## 10. loop_exit 验证方式

- `run loop_exit`：验证前台程序正常退出并返回 shell
- `start loop_exit`：验证后台程序退出后可由后续 `wait` 或 init/reaper 回收
- `wait`：验证 shell 手动回收已经退出的后台子进程

## 11. 父子关系与 reparent

当前 MiniOS 的用户程序不是平铺在进程表里，而是保留最小父子关系：

```text
init(PPID=0)
    -> shell
        -> hello / echo / loop / loop_exit / sleep_test
```

当前语义是：

- init 是根进程，`PPID=0`
- shell 由 init 创建，`PPID` 指向 init
- shell 的 `run/start` 会 `fork` 出子进程，因此用户程序的 `PPID` 指向 shell
- 普通 `wait` 只回收当前 shell 名下已经退出的子进程
- 如果父进程先退出，仍存在的子进程会被 reparent 给 init
- 被 reparent 给 init 的孤儿进程退出后，由 init/reaper 兜底回收

`ps` 输出中的 `PPID` 列就是观察这条关系的主要方式。

## 12. shell kill 命令

当前 shell 支持最小进程控制命令：

```text
kill <pid>
```

典型验证方式是：

```text
start loop
ps
kill <loop_pid>
wait
ps
```

当前 `kill` 的语义是：

- 通过 pid 查找目标进程
- 允许终止的目标会被标记为 `ZOMBIE`
- `EXIT` 列会显示教学版 kill 退出状态
- 目标进程不再被调度器选中
- 后续由 `wait` 或 init/reaper 回收

安全限制：

- 不允许 kill init
- 不允许当前 shell 直接 kill 自己
- `kill abc` 会报 `Invalid pid`
- `kill 9999` 这类不存在 pid 会报 `Kill failed`
- 当前不支持 `kill -9` 或其它信号编号

## 13. shell jobs 命令

当前 shell 支持最小后台任务观察命令：

```text
jobs
```

典型使用方式：

```text
start loop
jobs
ps
kill <loop_pid>
jobs
wait
jobs
```

`jobs` 和 `ps` 的区别是：

- `ps`：显示系统全局进程表，包括 init、shell 和普通用户程序
- `jobs`：只显示当前 shell 通过 `start` 创建的后台子进程

当前 `jobs` 显示字段：

- `JOB`：本次遍历临时生成的教学版显示编号
- `PID`：后台任务进程 pid
- `STATE`：当前进程状态
- `NAME`：程序名

`jobs` 只负责观察，不负责真正回收进程。后台任务退出或被 kill 后，仍需要通过 `wait` 或 init/reaper 完成回收。

## 14. uptime / sleep / sleep_test

当前时间相关用户态体验保持教学版最小模型：

- `uptime` / `ticks`：显示系统启动以来累计 tick
- 当前 shell 还会额外显示按 `20Hz` 换算后的整秒数
- `sleep <ticks>`：让当前 shell 睡眠若干个 tick
- `sleep_test`：循环输出 sleep 前后 tick，用于观察 `SLEEPING -> READY` 的唤醒路径

典型验证方式：

```text
uptime
sleep 100
uptime
run sleep_test
start sleep_test
ps
```

当前时间单位说明：

- 内核内部统一使用 tick
- 当前 PIT 默认频率是 `20Hz`
- 因此 `1 tick ≈ 50ms`
- `sleep` 参数单位不是秒，而是 tick

## 15. ls / cat 与内置只读文件

当前 shell 还没有真实文件系统，但已经支持最小只读文件观察：

- `ls`：列出当前内置只读文件
- `cat <file>`：输出指定内置只读文件内容

当前内置文件包括：

- `/readme.txt`
- `/programs`
- `/help.txt`

当前 shell 还做了一个最小兼容：

- `/readme.txt`
- `readme.txt`
- `readmetxt`

都可以匹配到同一份内置文件；这样在教学环境里即使不方便输入 `/` 或 `.`，也仍能完成 `cat` 验证。

典型用法：

```text
ls
cat /readme.txt
cat /programs
cat /help.txt
```

当前 `ls/cat` 的定位是教学版文件抽象：

- 文件来自内核静态字符串
- 不来自真实磁盘
- 不支持写入
- 不支持目录树
- 不支持 `open/read/close` syscall

## 16. cat 与 fd 层

在 Task58 中，`cat <file>` 已经优先改成：

```text
open(path)
    -> read(fd, buf, size)
        -> close(fd)
```

也就是说，用户看到的 `cat /readme.txt` 效果保持不变，但内部已经开始复用教学版 fd 语义。

## 17. 用户态 cat 程序

在 Task59 中，MiniOS 新增了用户态 `cat` 程序。

它和 shell 内建 `cat` 的区别是：

- `cat /readme.txt`
  - 走 shell 内建命令
- `run cat /readme.txt`
  - 走用户态程序

用户态 `cat` 的最小链路是：

```text
run cat /readme.txt
    -> shell 解析 argv
    -> exec 启动 cat
    -> SYS_OPEN
    -> SYS_READ
    -> SYS_WRITE
    -> SYS_CLOSE
```

因此，文件访问不再只是 shell 自己的内建逻辑，而开始具备“用户程序通过 syscall 访问文件”的雏形。

## 18. 用户态 ls 程序

在 Task60 中，MiniOS 继续新增了用户态 `ls` 程序。

它和 shell 内建 `ls` 的区别是：

- `ls`
  - 走 shell 内建命令
- `run ls`
  - 走用户态程序

用户态 `ls` 的最小链路是：

```text
run ls
    -> shell 解析程序名
    -> exec 启动 ls
    -> SYS_FILE_COUNT
    -> SYS_FILE_INFO
    -> SYS_WRITE
```

因此，文件系统不再只是“能读内容”，也开始具备“用户程序通过 syscall 列出文件元信息”的能力。

## 19. 用户态 stat 程序

在 Task61 中，MiniOS 新增了用户态 `stat` 程序。

典型用法：

```text
run stat /readme.txt
run stat /programs
```

它的最小链路是：

```text
run stat /readme.txt
    -> shell 解析 argv
    -> exec 启动 stat
    -> SYS_STAT(path, stat_buf)
    -> SYS_WRITE 输出 Name/Size/Type
```

当前输出字段是：

- `Name`
- `Size`
- `Type`

其中当前 `Type` 最小支持：

- `readonly-file`
- `ramfs-file`

## 20. RAMFS 文件与用户态程序

在 Task62 中，MiniOS 继续把文件系统从“只读文件表”推进到“内存可写文件”。

当前 shell 新增命令：

- `touch <file>`
- `writefile <file> <text>`
- `rm <file>`

典型验证链路：

```text
touch /note.txt
writefile /note.txt hello
cat /note.txt
run cat /note.txt
run stat /note.txt
run ls
rm /note.txt
```

其中：

- shell 内建 `cat` 现在可以读取 RAMFS 文件
- 用户态 `run cat` 通过已有 `open/read/close` syscall 读取 RAMFS 文件
- 用户态 `run ls` 会把 RAMFS 文件和内置只读文件一起列出来
- 用户态 `run stat` 会把 RAMFS 文件显示为 `ramfs-file`

## 21. 用户态 writefile 与 RAMFS fd 写入

在 Task63 中，MiniOS 继续把 RAMFS 写入能力从 shell 内建命令推进到用户态程序。

当前新增用户态程序：

- `run writefile /note.txt hello`

这条链路不再直接调用 shell 内建 RAMFS 写接口，而是通过 syscall 走最小 fd 写入路径：

```text
run writefile /note.txt hello
    -> sys_open_write(path)
    -> sys_fd_write(fd, text, size)
    -> sys_close(fd)
```

这意味着当前同时存在两种写法：

```text
writefile /note.txt hello
```

- shell 内建命令

```text
run writefile /note.txt hello
```

- 用户态程序，通过 syscall/fd 写 RAMFS

当前规则：

1. 用户态 `writefile` 只允许写 RAMFS 文件
2. 对 `/readme.txt` 这类内置只读文件写入会失败
3. 推荐先 `touch` 再写入
4. 当前采用覆盖写，不支持 append
5. 当前只处理简单文本参数，不做复杂引号解析

## 22. 当前限制

1. 暂不支持磁盘文件系统
2. 暂不支持 `PATH`
3. 暂不支持动态加载外部 ELF 文件
4. 暂不支持 `envp`
5. 暂不支持复杂引号、转义、管道和重定向
6. 当前 `argv` 保存在 PCB 暂存区里，还不是真实用户栈布局
7. 当前 `wait` / `waitpid` 仍是教学版最小实现，不等价于完整 Linux `waitpid`
8. 暂不支持信号、进程组、session 和 TTY 控制
9. 当前 reparent 只维护 `parent_pid`，不维护完整子链表
10. 当前 kill 不是完整信号系统，不支持进程组 kill 和权限模型
11. 当前 jobs 不是完整 job control，不支持 `fg/bg` 和 Ctrl+Z
12. 当前 uptime 不是 RTC / wall clock，不显示真实日期时间
13. 当前 RAMFS 不持久化，重启后文件丢失
14. 当前 RAMFS 不支持 append 或复杂并发写保护
15. 当前 fd 写入只支持 RAMFS 文件，不支持 pipe、dup/dup2 和重定向
16. 用户态 `cat` 仍只支持单文件、只读、无重定向的教学版语义
17. 用户态 `writefile` 当前只支持简单文本参数，不支持复杂引号解析
18. 内置程序镜像仍由内核预先编译并嵌入
