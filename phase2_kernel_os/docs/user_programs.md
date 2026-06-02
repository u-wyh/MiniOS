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
- `PROGRAM_WRITEFILE = 15`
- `PROGRAM_APPEND = 16`
- `PROGRAM_WC = 17`
- `PROGRAM_GREP = 18`
- `PROGRAM_HEAD = 19`
- `PROGRAM_TAIL = 20`
- `PROGRAM_SORT = 21`
- `PROGRAM_PIPE_TEST = 22`
- `PROGRAM_DUP2_TEST = 23`
- `PROGRAM_FORK_FD_TEST = 24`
- `PROGRAM_PIPE_FORK_DUP2_TEST = 25`

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
- `writefile -> PROGRAM_WRITEFILE`
- `append -> PROGRAM_APPEND`
- `wc -> PROGRAM_WC`
- `grep -> PROGRAM_GREP`
- `head -> PROGRAM_HEAD`
- `tail -> PROGRAM_TAIL`
- `sort -> PROGRAM_SORT`
- `pipe_test -> PROGRAM_PIPE_TEST`
- `dup2_test -> PROGRAM_DUP2_TEST`
- `fork_fd_test -> PROGRAM_FORK_FD_TEST`
- `pipe_fork_dup2_test -> PROGRAM_PIPE_FORK_DUP2_TEST`

其中 shell 默认直接暴露给用户的程序主要是：

- `hello`
- `echo`
- `ls`
- `cat`
- `stat`
- `writefile`
- `append`
- `wc`
- `grep`
- `head`
- `tail`
- `sort`
- `pipe_test`
- `dup2_test`
- `fork_fd_test`
- `pipe_fork_dup2_test`
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
4. 当前 `writefile` 采用覆盖写，`append` 采用追加写
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
14. 当前 RAMFS 已支持最小 append，但仍不支持复杂并发写保护
15. 当前 fd 写入只支持 RAMFS 文件，不支持 pipe、dup/dup2 和重定向
16. 用户态 `cat` 仍只支持单文件、只读、无重定向的教学版语义
17. 用户态 `writefile` 当前只支持简单文本参数，不支持复杂引号解析
18. 内置程序镜像仍由内核预先编译并嵌入

## 22. 用户态 append 程序

当前新增用户态程序：

- `run append /note.txt world`

这条链路不再直接调用 shell 内建 RAMFS 追加接口，而是通过 syscall 走最小 append 写入路径：

```text
run append /note.txt world
    -> SYS_APPEND_FILE(path, text)
```

当前同时存在两种追加写法：

```text
append /note.txt world
```

- shell 内建命令

```text
run append /note.txt world
```

- 用户态程序，通过 syscall 追加写入 RAMFS

当前规则：

1. 用户态 `append` 只允许追加到 RAMFS 文件
2. 对 `/readme.txt` 这类内置只读文件 append 会失败
3. 推荐先 `touch` 再写入或追加
4. 当前不自动添加空格和换行
5. 当前只处理简单文本参数，不做复杂引号解析

## 23. Shell echo 重定向到 RAMFS

Task65 当前新增的是 shell 语法层重定向，而不是新的用户程序。

当前支持：

```text
echo hello > /note.txt
echo world >> /note.txt
```

语义是：

1. `>`：覆盖写入 RAMFS 文件
2. `>>`：追加写入 RAMFS 文件

这里要和已有命令区分清楚：

```text
writefile /note.txt hello
```

- shell 内建覆盖写

```text
run writefile /note.txt hello
```

- 用户态 writefile 程序

```text
append /note.txt world
```

- shell 内建追加写

```text
run append /note.txt world
```

- 用户态 append 程序

而 Task65 新增的是：

```text
echo hello > /note.txt
echo world >> /note.txt
```

当前限制：

1. 只支持 `echo` 的重定向
2. 不支持 `run cat /readme.txt > /copy.txt`
3. 不支持通用用户程序 stdout 捕获
4. 不支持管道与重定向组合

## 24. run 用户程序 stdout 重定向到 RAMFS

Task66 当前新增的是 `run` 启动用户程序的 stdout 重定向。

当前支持：

```text
run cat /readme.txt > /copy.txt
run ls > /files.txt
run stat /readme.txt > /stat.txt
```

如需追加：

```text
run stat /programs >> /stat.txt
```

这里要和 Task65 的 echo 专用重定向区分：

```text
echo hello > /note.txt
```

- shell 直接写 RAMFS

```text
run cat /readme.txt > /copy.txt
```

- 用户程序照常调用 `sys_write`
- 内核根据当前进程的 stdout 重定向配置，把输出写入 RAMFS

当前规则：

1. 普通 `run cat /readme.txt` 仍输出到屏幕
2. `run ... > file`
   - 第一次 `SYS_WRITE` 覆盖写
   - 后续多次 `SYS_WRITE` 自动追加
3. `run ... >> file`
   - 目标文件必须已存在
   - 所有输出都按追加写处理
4. 只读内置文件不能作为重定向目标
5. 当前不支持 `start ... > file`
6. 当前不支持管道和复杂重定向组合

## 25. run 用户程序 stdin 重定向到文件

Task67 当前新增的是 `run ... < file`。

最小支持示例：

```text
run cat < /readme.txt
run cat < /programs
run cat < /input.txt
```

当前用户态 `cat` 的两种模式是：

```text
run cat /readme.txt
```

- argv 文件模式：仍通过 `open/read/close` 读取指定文件

```text
run cat < /readme.txt
```

- stdin 模式：当没有文件参数时，`cat` 会循环 `sys_read(0, ...)`，直到 EOF

当前规则：

1. `<` 和目标文件不会传进用户程序 argv
2. 输入文件既可以是内置只读文件，也可以是 RAMFS 文件
3. 普通 `run cat /readme.txt` 不受影响
4. 普通 `run cat` 在没有 stdin 重定向时会安全结束，不崩溃
5. 当前不支持或暂不推荐 `<` 与 `>` 组合
6. 当前不支持 `run cat /readme.txt < /input.txt` 这种 argv 文件模式和 stdin 重定向混用

## 26. run 用户程序组合重定向

Task68 当前把 stdin 和 stdout 重定向组合起来，支持：

```text
run cat < /readme.txt > /copy.txt
run cat < /input.txt > /output.txt
run cat < /input.txt >> /log.txt
```

当前规则：

1. `<`、`>`、`>>` 以及后面的路径都不会传入用户程序 argv
2. `cat` 在没有 argv 文件名时会从 `fd=0` 读取
3. `SYS_WRITE` 会根据 stdout 重定向配置把输出写到目标文件
4. `>`：
   - 不存在则自动创建 RAMFS 文件
   - 存在则覆盖写
5. `>>`：
   - 目标文件必须已存在
   - 目标文件必须是 RAMFS 文件
6. 只读内置文件不能作为输出目标
7. 当前仍不支持：
   - 管道
   - stderr 重定向
   - 多个输入/输出重定向
   - 复杂 quoting

当前最典型示例：

```text
run cat < /readme.txt > /copy.txt
cat /copy.txt
```

## 27. run 用户程序教学版单管道

Task69 当前支持：

```text
run cat /readme.txt | run cat
run cat /programs | run cat
run cat /input.txt | run cat
```

当前规则：

1. 只支持单个 `|`
2. 左右两侧都必须是 `run`
3. 左侧程序先完整运行
4. 左侧 stdout 写入教学版 pipe buffer
5. 右侧程序再运行
6. 右侧 stdin 从 pipe buffer 读取

因此：

```text
run cat /readme.txt | run cat
```

当前右侧 `cat` 会以 stdin 模式读取 pipe 内容，而不是直接从文件读取。

当前仍不支持：

1. 多级管道
2. 管道和重定向组合
3. 后台管道
4. 复杂 quoting

## 28. run 用户程序 pipe + stdout 重定向

Task70 当前把 Task69 的单管道和 Task66 的 stdout 文件重定向组合起来，支持：

```text
run cat /readme.txt | run cat > /copy.txt
run cat /programs | run cat > /programs_copy.txt
run cat /programs | run cat >> /log.txt
```

当前规则：

1. 左侧程序仍然把 stdout 写入 pipe buffer
2. 右侧程序仍然从 stdin 读取 pipe 内容
3. 右侧程序的 stdout 不再直接输出到屏幕
4. 右侧程序的 stdout 会写入目标 RAMFS 文件
5. `>` 表示覆盖写
6. `>>` 表示追加写

因此：

```text
run cat /readme.txt | run cat > /copy.txt
cat /copy.txt
```

当前等价于把左侧 `cat` 的输出，经由右侧 `cat` 再落到 RAMFS 文件。

当前仍不支持：

1. 多级管道
2. `run cat < /readme.txt | run cat > /x.txt`
3. 后台管道
4. 复杂 quoting

## 29. run 用户程序 pipe + stdin 重定向

Task71 当前把 Task67 的文件 stdin 重定向和 Task69 的单管道组合起来，支持：

```text
run cat < /readme.txt | run cat
run cat < /programs | run cat
run cat < /input.txt | run cat
```

当前规则：

1. `<` 和输入文件路径不会传入左侧用户程序 argv
2. `|` 不会传入左右任一用户程序 argv
3. 左侧程序在没有 argv 文件名时，会进入 stdin 模式
4. 左侧程序通过 `sys_read(0, ...)` 从输入文件读取
5. 左侧程序通过 `sys_write(...)` 把输出写入 pipe buffer
6. 右侧程序继续以 stdin 模式从 pipe buffer 读取
7. 右侧程序继续正常输出到屏幕

因此：

```text
run cat < /readme.txt | run cat
```

当前等价于把 `/readme.txt` 内容先喂给左侧 `cat`，再由右侧 `cat` 从 pipe stdin 接着读出并显示。

当前仍不支持：

1. `run cat /readme.txt < /input.txt | run cat`
2. `run cat < /readme.txt | run cat > /x.txt`
3. 多级管道
4. 后台管道
5. 复杂 quoting

## 30. run 用户程序完整单管道数据流

Task72 当前把 Task71 和 Task70 组合起来，支持：

```text
run cat < /readme.txt | run cat > /copy.txt
run cat < /programs | run cat > /programs_copy.txt
run cat < /input.txt | run cat > /output.txt
run cat < /input.txt | run cat >> /log.txt
```

当前规则：

1. `<` 和输入文件路径不会传入左侧用户程序 argv
2. `>` / `>>` 和输出文件路径不会传入右侧用户程序 argv
3. `|` 不会传入左右任一用户程序 argv
4. 左侧程序在没有 argv 文件名时，会从 `fd=0` 读取输入文件
5. 左侧程序的 `sys_write(...)` 会把输出写入 pipe buffer
6. 右侧程序继续通过 `sys_read(0, ...)` 从 pipe buffer 读取
7. 右侧程序的 `sys_write(...)` 会把输出写入目标 RAMFS 文件

因此：

```text
run cat < /readme.txt | run cat > /copy.txt
cat /copy.txt
```

当前等价于把 `/readme.txt` 的内容经由两个用户态程序后再写入 RAMFS 文件。

当前仍不支持：

1. 多级管道
2. 后台管道
3. stderr 重定向
4. 复杂 quoting

## 31. 用户态 wc 程序

Task73 当前新增了一个最小用户态 `wc` 程序，用来验证 stdin / pipe / stdout redirect 数据流。

当前用法：

```text
run wc
run wc < /readme.txt
run cat /readme.txt | run wc
run cat < /input.txt | run wc > /count.txt
```

当前输出格式：

```text
bytes: 80
lines: 2
words: 14
```

当前语义：

1. `wc` 不读取 argv 文件路径
2. `wc` 统一通过 `sys_read(0, ...)` 从 stdin 读取
3. 当 stdin 没有重定向时，当前会安全读到 EOF 并输出 0 统计
4. `wc` 的结果通过 `sys_write(...)` 输出，因此既可以显示到屏幕，也可以继续重定向到 RAMFS 文件

## 32. 用户态 grep 程序

Task74 当前新增了一个最小用户态 `grep` 程序，用来验证 stdin / pipe / stdout redirect 下的数据过滤。

当前用法：

```text
run grep MiniOS
run grep MiniOS < /readme.txt
run cat /readme.txt | run grep MiniOS
run cat < /readme.txt | run grep MiniOS > /grep.txt
```

当前语义：

1. `grep` 的第一个参数作为关键字
2. `grep` 当前不读取 argv 文件路径
3. `grep` 统一通过 `sys_read(0, ...)` 从 stdin 读取
4. `grep` 会按行判断是否包含关键字，并输出匹配行
5. `grep` 当前采用教学版 ASCII 大小写无关匹配
6. `grep` 的结果通过 `sys_write(...)` 输出，因此既可以显示到屏幕，也可以继续重定向到 RAMFS 文件

## 33. 用户态 head 程序

Task75 当前新增了一个最小用户态 `head` 程序，用来验证 stdin / pipe / stdout redirect 下的数据截断。

当前用法：

```text
run head
run head < /readme.txt
run head -n 3 < /readme.txt
run cat /readme.txt | run head
run cat < /readme.txt | run head -n 3 > /head.txt
```

当前语义：

1. `head` 默认输出前 10 行
2. `head -n N` 会输出前 N 行
3. `head` 当前不读取 argv 文件路径，统一从 stdin 读取
4. `head` 通过 `sys_read(0, ...)` 兼容文件 stdin 与 pipe stdin
5. `head` 通过 `sys_write(...)` 输出，因此既可以显示到屏幕，也可以继续重定向到 RAMFS 文件
6. `head -n 0` 会正常不输出并退出
7. 当前不支持多个文件参数，也不支持完整 GNU `head` 参数

## 34. 用户态 tail 程序

Task76 当前新增了一个最小用户态 `tail` 程序，用来验证 stdin / pipe / stdout redirect 下的“尾部截断”。

当前用法：

```text
run tail
run tail < /readme.txt
run tail -n 3 < /readme.txt
run cat /readme.txt | run tail
run cat < /readme.txt | run tail -n 3 > /tail.txt
```

当前语义：

1. `tail` 默认输出最后 10 行
2. `tail -n N` 会输出最后 N 行
3. `tail -n 0` 会正常不输出并退出
4. `tail` 当前不读取 argv 文件路径，统一从 stdin 读取
5. `tail` 通过 `sys_read(0, ...)` 兼容文件 stdin 与 pipe stdin
6. `tail` 在用户态内部使用固定缓冲区缓存最近输入，再从后往前找最后 N 行
7. `tail` 通过 `sys_write(...)` 输出，因此既可以显示到屏幕，也可以继续重定向到 RAMFS 文件
8. 当前不支持多个文件参数，不支持完整 GNU `tail` 参数，也不支持 `-f`
9. 当前使用固定缓冲区，因此只保证窗口范围内的最后 N 行

与 `head` 的区别：

1. `head` 可以边读边截断前 N 行
2. `tail` 需要先读完输入，再决定最后 N 行从哪里开始输出

## 35. 用户态 sort 程序

Task77 当前新增了一个最小用户态 `sort` 程序，用来验证 stdin / pipe / stdout redirect 下的“整段缓存、切行、排序、重组输出”。

当前用法：

```text
run sort
run sort < /readme.txt
run cat /readme.txt | run sort
run sort < /readme.txt > /sorted.txt
run cat < /readme.txt | run sort > /sorted2.txt
```

当前语义：

1. `sort` 当前不接收额外参数，统一从 stdin 读取
2. `sort` 会把输入按 `\n` 切成若干行
3. `sort` 会按字节字典序升序排列每一行
4. 当前比较规则是逐字符比较；前缀相同时较短行排在前面
5. `sort` 通过 `sys_read(0, ...)` 兼容文件 stdin 与 pipe stdin
6. `sort` 通过 `sys_write(...)` 输出，因此既可以显示到屏幕，也可以继续重定向到 RAMFS 文件
7. 最后一行即使没有换行，也会参与排序并正常输出
8. 空输入时会正常退出
9. 输入过大或行数过多时会给出简单错误提示，不做复杂外部排序

当前限制：

1. 不支持多个文件参数
2. 不支持完整 GNU `sort` 参数
3. 不支持 `-r`
4. 不支持 `-n`
5. 不支持去重
6. 不支持 locale 相关排序
7. 使用固定缓冲区和固定最大行数

与 `head` / `tail` / `grep` / `wc` 的区别：

1. `wc` 只做统计，不重排输入
2. `grep` 只做按行过滤，不改变保留下来的行顺序
3. `head` / `tail` 只做截断
4. `sort` 需要先缓存全部输入，再切行、比较并交换行顺序

## 36. 用户态 pipe_test 程序

Task84 新增了最小用户态 `pipe_test` 程序，用来验证教学版 `pipe()` syscall。

当前用法：

```text
run pipe_test
```

当前语义：

1. `pipe_test` 会先调用 `pipe(fds)`
2. 成功时会输出读端 fd 和写端 fd
3. 然后向 `fds[1]` 写入 `hello pipe\n`
4. 再从 `fds[0]` 读取并输出读回的数据
5. 最后再读一次，验证 EOF 返回值

它的主要作用是验证：

1. pipe syscall 能返回一对 fd
2. `FD_PIPE_WRITE` 可以被 `write`
3. `FD_PIPE_READ` 可以被 `read`
4. 当前教学版 pipe 的 EOF 语义仍然成立

## 37. 用户态 dup2_test 程序

Task85 新增了最小用户态 `dup2_test` 程序，用来验证教学版 `dup2()` syscall。

当前用法：

```text
run dup2_test
```

当前语义：

1. `dup2_test` 会先调用 `pipe(fds)`
2. 然后执行 `dup2(fds[1], 5)`，把 pipe 写端复制到普通 fd `5`
3. 通过 `write(5, ...)` 验证复制后的 pipe write fd 可用
4. 再执行 `dup2(fds[0], 6)`，把 pipe 读端复制到普通 fd `6`
5. 通过 `read(6, ...)` 验证复制后的 pipe read fd 可用
6. 同时验证：
   - `dup2(valid_fd, valid_fd)` 的稳定返回
   - `dup2(-1, 5)` / `dup2(valid_fd, -1)` / `dup2(valid_fd, 99)` 的最小错误路径

它的主要作用是验证：

1. 用户态已经可以直接调用 `dup2()`
2. `dup2` 当前可以复制 pipe write fd
3. `dup2` 当前可以复制 pipe read fd
4. `dup2` 当前最小错误路径不会导致 panic

## 38. 用户态 fork_fd_test 程序

Task86 新增了最小用户态 `fork_fd_test` 程序，用来验证 fork 后 pipe fd 继承。

当前用法：

```text
run fork_fd_test
```

当前语义：

1. `fork_fd_test` 会先调用 `pipe(fds)`
2. 然后执行 `fork()`
3. 子进程使用继承下来的 `fds[1]` 写入 `child says hello\n`
4. 父进程 `waitpid(child_pid)` 后，再从 `fds[0]` 读取数据
5. 读到预期文本后输出 `fork_fd_test: ok`

它的主要作用是验证：

1. fork 后子进程已经继承父进程的 pipe fd
2. 子进程退出不会错误清空父进程还要读取的 pipe 数据
3. 当前教学版 fd 继承已经足以支撑最小父子 pipe 传递

## 39. 用户态 pipe_fork_dup2_test 程序

Task87 新增了最小用户态 `pipe_fork_dup2_test` 程序，用来验证用户态自己组合 `pipe + fork + dup2`。

当前用法：

```text
run pipe_fork_dup2_test
```

当前语义：

1. 父进程先调用 `pipe(fds)`
2. 父进程再调用 `fork()`
3. 子进程执行 `dup2(fds[1], 1)`，把 stdout 接到 pipe 写端
4. 子进程通过 `write(1, ...)` 把消息写入 pipe
5. 父进程 `waitpid()` 后执行 `dup2(fds[0], 0)`，把 stdin 接到 pipe 读端
6. 父进程通过 `read(0, ...)` 把消息读回
7. 成功后输出 `pipe_fork_dup2_test: ok`

它的主要作用是验证：

1. 用户态已经可以自己组合 `pipe + fork + dup2`
2. 子进程可以通过 dup2 后的 `stdout` 把文本写入 pipe
3. 父进程可以通过 dup2 后的 `stdin` 把文本读回
4. 当前这套教学版机制已经能形成最小闭环
