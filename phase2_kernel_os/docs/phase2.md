# MiniOS Phase2 进度补充

## Task49：系统调用表整理与文档化

- 整理了当前 syscall 编号分组和命名。
- 补充了 syscall 分发层的参数 / 返回值注释。
- 新增 `docs/syscall.md`，集中记录当前 Phase2 的 syscall ABI。

## Task50：用户程序表 / program_id 整理

- 统一了内置用户程序 `program_id`。
- 统一了 `program name -> program_id` 映射。
- 统一了 `program_id -> ELF/blob` 查询入口。
- shell `run/start` 与内核 `exec` 复用同一份用户程序表。
- 当前仍不支持真实文件系统、`PATH` 搜索和外部 ELF 动态加载。

## Task51：用户程序参数传递整理 / argc argv 语义统一

- 明确了 shell `run/start` 到 `exec` 的参数传递语义。
- 当前保留教学版 `argv[0] = program name`，其余 token 作为用户参数。
- 统一了参数数量上限与单参数长度上限，并在 shell / process 两侧共同校验。
- `echo` 可继续用于验证 `argc/argv` 传递，`hello` / `loop` / `loop_exit` / `sleep_test` 路径保持兼容。
- 当前仍不支持 `envp`、`PATH`、复杂引号、转义和真实文件系统加载。

## Task52：用户程序退出状态 / wait 语义整理

- 明确了 `exit(status)` 后进程进入 `ZOMBIE`、等待父进程或 init/reaper 回收的最小语义。
- `run`、`start`、`wait` 的关系进一步清晰：前台默认等待，后台默认不等待，`wait` 负责手动回收已退出子进程。
- `ps` 现在会显示每个进程当前记录的 `exit_status`，便于观察 zombie/kill 后的退出码。
- 当前仍然不是完整 Linux `waitpid` / 信号 / 进程组模型，只保留教学版闭环。

## Task53：进程父子关系 / reparent 语义整理

- 明确了 `parent_pid` 语义：init 使用 `PPID=0`，shell 由 init 创建，shell 启动的用户程序属于 shell 子进程。
- 父进程退出或被 `kill` 时，其仍存在的子进程会被 reparent 给 init，避免 `parent_pid` 指向已释放进程。
- `wait` / `waitpid` / `wait_any` 只回收当前进程名下的子进程；init/reaper 只兜底清理孤儿 zombie。
- `ps` 继续通过 `PPID` 展示父子关系，便于观察 `start loop`、`start loop_exit`、`wait` 等场景。
- 当前仍不实现完整进程树、信号、进程组、session 或复杂权限模型。

## Task54：kill syscall / shell kill 命令整理

- 整理了 `SYS_KILL(pid)` 与 shell `kill <pid>` 的最小教学版语义。
- `kill` 成功后目标进程进入 `ZOMBIE`，退出状态记录为 `PROCESS_KILL_EXIT_STATUS`。
- 被 kill 的进程不再被调度器选中，后续仍通过父进程 `wait/waitpid/wait_any` 或 init/reaper 回收。
- 当前明确拒绝 kill init，也拒绝当前 shell 直接 kill 自己。
- 当前 `kill` 不是完整 Unix/Linux 信号系统，不支持 `kill -9`、进程组 kill 或权限模型。

## Task55：Shell 前后台任务观察 / jobs 命令整理

- 新增 shell `jobs` 命令，用于观察当前 shell 直接管理的后台任务。
- `jobs` 基于 `parent_pid` 和 `is_background` 过滤，只显示由当前 shell `start` 出来的后台子进程。
- `jobs` 输出 `JOB / PID / STATE / NAME`，其中 `JOB` 是当前遍历生成的教学版显示编号。
- `jobs` 不负责回收资源；后台任务退出或被 kill 后，仍由 `wait` 或 init/reaper 回收。
- 当前不实现完整 job control，不支持 `fg/bg`、Ctrl+Z、进程组、session 或 tty 前台控制。

## Task56：系统 tick / sleep / uptime 语义整理

- 统一确认 `pit_get_ticks()` 是当前系统 tick 的只读查询入口。
- 新增 `pit_get_frequency()`，明确当前 PIT 默认频率为 `20Hz`，即 `1 tick ≈ 50ms`。
- `sleep(ticks)` 继续以 tick 为单位；`sleep(0)` 退化为最小 `yield`。
- `uptime` / `ticks` 命令现在可显示累计 tick 和按 `20Hz` 换算后的整秒数。
- `sleep_test` 输出 sleep 前后 tick，便于观察 `SLEEPING -> READY` 的唤醒路径。

## Task57：内核内置只读文件表 / ls、cat 雏形

- 当前文件系统仍不是磁盘文件系统，而是教学版内置只读文件表。
- 共享文件清单统一维护了最小文件对象：路径、内容和大小。
- shell 新增 `ls`，用于列出内置只读文件。
- shell 新增 `cat <file>`，用于输出指定内置只读文件内容。
- 当前内置文件包括 `/readme.txt`、`/programs`、`/help.txt`。
- 这一步的目标是为后续 `open/read/close` syscall 预热，而不是直接实现真实 VFS/磁盘驱动。

## Task58：只读文件描述符 / open-read-close syscall 雏形

- 当前在内置只读文件表基础上继续引入教学版 fd。
- fd 表项最小记录：
  - 是否占用
  - 指向哪个只读文件
  - 当前读取 offset
- 当前已支持：
  - `open(path)`
  - `read(fd, buf, size)`
  - `close(fd)`
- `cat <file>` 已优先通过 fd 层读取，而不是直接输出静态字符串。
- 当前仍不是完整文件系统，不涉及真实磁盘、写入、目录树、inode 或 block cache。

## Task59：用户态 cat 程序 / open-read-close syscall 对接

- 当前继续把 Task58 的 fd 层暴露给用户态程序。
- 新增用户态 `cat`，支持 `run cat /readme.txt`。
- 用户态 `cat` 通过 `sys_open -> sys_read -> sys_write -> sys_close` 访问内置只读文件。
- shell 内建 `cat` 仍保留，因此：
  - `cat /readme.txt` 是 shell 内建命令
  - `run cat /readme.txt` 是用户态程序链路
- 这一步让文件访问不再只是 shell 内建功能，而开始具备用户态文件 syscall 雏形。

## Task60：用户态 ls 程序 / 文件列表 syscall 对接

- 当前继续把文件列表能力从 shell 内建命令推进到用户态程序。
- 新增用户态 `ls`，支持 `run ls`。
- 新增教学版文件列表 syscall：
  - `SYS_FILE_COUNT`
  - `SYS_FILE_INFO`
- 用户态 `ls` 通过 syscall 枚举内置只读文件的路径和大小，而不是直接访问内核文件表。
- shell 内建 `ls` 继续保留，因此：
  - `ls` 是 shell 内建命令
  - `run ls` 是用户态程序链路
- 当前仍不是真实目录系统，只是在内置只读文件表基础上提供最小文件列表查询能力。

## Task61：文件 stat syscall / 用户态 stat 程序

- 当前继续把文件系统元信息查询能力推进到用户态程序。
- 新增教学版 `SYS_STAT`，按路径查询单个内置只读文件的基础元信息。
- 新增用户态 `stat`，支持 `run stat /readme.txt` 与 `run stat /programs`。
- 当前教学版 `stat` 结构只返回：
  - `size`
  - `type`
- 当前类型统一使用 `readonly-file`，不引入 inode、权限、uid/gid、时间戳或 block 数等复杂字段。
- 这一步让 MiniOS 文件接口进一步形成闭环：
  - `ls`：列出文件
  - `cat`：读取内容
  - `stat`：查询元信息

## Task62：RAMFS 可写内存文件系统雏形 / touch、writefile、rm

- 当前在内置只读文件表之外，再增加了一张教学版 RAMFS 内存文件表。
- RAMFS 文件全部驻留内存，系统重启后丢失，不涉及真实磁盘或持久化。
- shell 新增：
  - `touch <file>`
  - `writefile <file> <text>`
  - `rm <file>`
- `ls` / `cat` / `stat` 以及 `run ls` / `run cat` / `run stat` 现在都能观察 RAMFS 文件。
- 当前写入语义保持最小化：只支持覆盖写入文本，不支持 append，不支持 `write(fd)`。

## Task63：RAMFS fd 写入 / write syscall 雏形

- 当前继续承接 Task62，把 RAMFS 写入能力从 shell 内建命令推进到 fd / syscall 层。
- 新增用户态 `writefile`，支持：
  - `run writefile /note.txt hello`
- 新增教学版写入 syscall：
  - `SYS_OPEN_WRITE`
  - `SYS_FD_WRITE`
- 当前用户态 `writefile` 通过：
  - `open_write`
  - `fd_write`
  - `close`
  这条链路写入 RAMFS 文件。
- shell 内建 `writefile` 继续保留，因此现在同时存在：
  - `writefile /note.txt hello`：内建命令
  - `run writefile /note.txt hello`：用户态程序
- 当前仍只允许写 RAMFS 文件，不允许修改内置只读文件。

## Task64：RAMFS append 追加写入 / 用户态 append 程序

- 当前继续承接 Task63，在覆盖写基础上补充 RAMFS 文件追加写入语义。
- shell 新增：
  - `append <file> <text>`
- 用户态新增：
  - `run append /note.txt world`
- 当前 append 语义是：
  - 从文件当前 `size` 位置继续写入
  - 保留旧内容
  - 更新新的 `size`
- 当前 `writefile` 与 `append` 的区别明确：
  - `writefile`：覆盖写入
  - `append`：追加写入
- 内置只读文件仍然禁止 append。
- 当前 append 仍是教学版最小实现，不支持完整 POSIX `O_APPEND`、并发原子追加和 `>>` 重定向。

## Task65：Shell 输出重定向到 RAMFS / > 与 >> 雏形

- 当前在已有 `writefile` / `append` 基础上，给 shell 增加了最小版输出重定向语法。
- shell 现在支持：
  - `echo <text> > <file>`
  - `echo <text> >> <file>`
- `>` 对应 RAMFS 覆盖写；目标文件不存在时，会自动创建 RAMFS 文件后再写入。
- `>>` 对应 RAMFS 追加写；目标文件必须已存在且必须是 RAMFS 文件。
- 当前只支持 `echo` 的输出重定向，不支持通用用户程序 stdout 重定向、管道组合与 `dup2`。

## Task66：用户态程序 stdout 重定向到 RAMFS / run ... > file

- 当前把 Task65 的 echo 专用重定向继续推进到 `run` 启动的用户态程序。
- shell 现在支持：
  - `run <program> [args] > <file>`
  - `run <program> [args] >> <file>`
- shell 会在创建子进程时把 stdout 重定向配置写入 PCB，用户程序本身无需感知重定向。
- `SYS_WRITE` 会根据当前进程是否启用 stdout 重定向，决定输出到屏幕还是 RAMFS。
- `>` 采用“第一次写覆盖、后续多次 `SYS_WRITE` 自动追加”的教学版语义，避免 `ls/stat/cat` 只留下最后一段输出。
- `>>` 当前对整个进程都采用追加写入语义。
- 当前仍不是完整 `dup2` / fd 复制模型，不支持 `<`、`2>`、`2>&1`、管道组合和后台任务重定向。

## Task67：用户态程序 stdin 重定向到 RAMFS / run ... < file

- 当前继续承接 Task66，把用户态程序的输入来源也推进到文件重定向模型。
- shell 现在支持：
  - `run cat < /readme.txt`
  - `run cat < /programs`
  - `run cat < /input.txt`
- shell 会在创建子进程时把 stdin 重定向配置写入 PCB，后续 `SYS_READ(fd=0)` 会改为从目标文件读取。
- 输入源既可以是内置只读文件，也可以是 RAMFS 文件。
- 用户态 `cat` 在没有 argv 文件名时，会进入 stdin 模式并循环读取 `fd=0` 直到 EOF。
- 普通 `run cat /readme.txt` 仍保持原有 argv 文件模式，不受 stdin 重定向实现影响。
- 当前仍不是完整 `dup2` / tty 模型，不支持真实键盘 stdin、here-doc、管道、多重输入重定向和 `<` 与 `>` 组合。

## Task68：组合重定向 < + > 雏形 / run ... < input > output

- 当前继续承接 Task66/67，把 stdin 与 stdout 重定向组合为单进程文件数据流。
- shell 现在支持：
  - `run cat < /readme.txt > /copy.txt`
  - `run cat < /input.txt > /output.txt`
  - `run cat < /input.txt >> /log.txt`
- shell 会在解析 `run` 时同时识别 `<` 与 `>` / `>>`，并把这些 token 从用户程序 argv 中剥离。
- 子进程可以同时启用：
  - `stdin_redirect_*`
  - `stdout_redirect_*`
- 组合重定向下的数据流是：
  - 输入文件 -> `SYS_READ(fd=0)` -> 用户程序 -> `SYS_WRITE` -> 输出文件
- 当前仍不是完整 `dup2` / pipe 模型，不支持 stderr、后台任务重定向、多重重定向和复杂 shell 语法。

## Task69：单管道 | 雏形 / 用户程序 stdout 接 stdin

- 当前继续承接 Task66～68，把“文件输入/输出重定向”再推进到“进程之间的教学版缓冲区连接”。
- shell 现在支持：
  - `run cat /readme.txt | run cat`
  - `run cat /programs | run cat`
  - `run cat /input.txt | run cat`
- 当前执行模型不是 UNIX 并发 pipe，而是顺序执行：
  - 左侧先完整运行
  - 左侧 stdout 写入教学版 pipe buffer
  - 右侧再运行
  - 右侧 stdin 从 pipe buffer 读取
- 当前 pipe buffer 不属于文件系统对象：
  - 不会出现在 `ls`
  - 不能被 `cat /path` 访问
- 当前仍不支持多级管道、管道与重定向组合、后台管道和复杂 shell 语法。

## Task70：管道 + 输出重定向组合雏形 / run A | run B > file

- 当前继续承接 Task69 和 Task66，把“进程到进程的数据流”继续推进到“进程到进程再到文件”。
- shell 现在支持：
  - `run cat /readme.txt | run cat > /copy.txt`
  - `run cat /programs | run cat > /programs_copy.txt`
  - `run cat /programs | run cat >> /log.txt`
- 当前执行模型仍是顺序执行：
  - 左侧先完整运行并把 stdout 写入 pipe buffer
  - 右侧再运行
  - 右侧 stdin 从 pipe buffer 读取
  - 右侧 stdout 再按 Task66 规则写入 RAMFS 文件
- 当前右侧进程可以同时启用：
  - `stdin <- pipe`
  - `stdout -> file`
- 当前仍不支持 stdin 重定向 + pipe、多级管道、后台管道和复杂 shell 组合。

## Task71：管道 + 输入重定向组合雏形 / run A < input | run B

- 当前继续承接 Task67 和 Task69，把“文件输入”接到管道左侧程序。
- shell 现在支持：
  - `run cat < /readme.txt | run cat`
  - `run cat < /programs | run cat`
  - `run cat < /input.txt | run cat`
- 当前执行模型仍是顺序执行：
  - 左侧先从文件读取 stdin
  - 左侧 stdout 写入 pipe buffer
  - 右侧再从 pipe buffer 读取并输出到屏幕
- 当前左侧进程可以同时启用：
  - `stdin <- file`
  - `stdout -> pipe`
- 当前右侧进程继续启用：
  - `stdin <- pipe`
- 当前仍不支持多级管道、后台管道和复杂 shell 组合。

## Task72：完整单管道数据流雏形 / run A < input | run B > output

- 当前继续承接 Task70 和 Task71，把“文件输入 -> 左进程 -> pipe -> 右进程 -> 文件输出”串起来。
- shell 现在支持：
  - `run cat < /readme.txt | run cat > /copy.txt`
  - `run cat < /programs | run cat > /programs_copy.txt`
  - `run cat < /input.txt | run cat > /output.txt`
  - `run cat < /input.txt | run cat >> /log.txt`
- 当前执行模型仍是顺序执行：
  - 左侧先从文件读取 stdin
  - 左侧 stdout 写入 pipe buffer
  - 右侧再从 pipe buffer 读取
  - 右侧 stdout 再写入 RAMFS 文件
- 当前左侧进程可以同时启用：
  - `stdin <- file`
  - `stdout -> pipe`
- 当前右侧进程可以同时启用：
  - `stdin <- pipe`
  - `stdout -> file`
- 当前仍不支持多级管道、后台管道和复杂 shell 组合。

## Task73：用户态 wc 程序 / stdin 数据流验证

- 当前新增了一个最小用户态 `wc` 程序，用来验证 “stdin -> 用户程序处理 -> stdout” 这条链路已经真正可用。
- `wc` 当前通过 `sys_read(0, ...)` 从 stdin 读取：
  - 文件 stdin 重定向
  - pipe stdin
- `wc` 当前通过 `sys_write(...)` 输出统计结果，因此可以继续走：
  - 屏幕输出
  - `stdout` 重定向写文件
- 当前最小验证链路包括：
  - `run wc < /readme.txt`
  - `run cat /readme.txt | run wc`
  - `run cat < /input.txt | run wc > /count.txt`
- 当前 `wc` 至少统计：
  - `bytes`
  - `lines`
  - `words`

## Task74：用户态 grep 程序 / pipe 文本过滤验证

- 当前新增了一个最小用户态 `grep` 程序，用来验证 “stdin -> 用户程序过滤 -> stdout” 这条链路已经真正可用。
- `grep` 当前第一个参数作为关键字，并通过 `sys_read(0, ...)` 从 stdin 读取：
  - 文件 stdin 重定向
  - pipe stdin
- `grep` 当前通过 `sys_write(...)` 输出包含关键字的整行，因此可以继续走：
  - 屏幕输出
  - `stdout` 重定向写文件
- 当前最小验证链路包括：
  - `run grep MiniOS < /readme.txt`
  - `run cat /readme.txt | run grep MiniOS`
  - `run cat < /readme.txt | run grep MiniOS > /grep.txt`
- 当前 `grep` 为教学版：
  - ASCII 大小写无关匹配
  - 不支持正则
  - 不支持多文件

## Task75：用户态 head 程序 / 读取前 N 行

- 当前新增了一个最小用户态 `head` 程序，用来验证 “stdin / pipe -> 用户程序截断 -> stdout” 这条链路已经真正可用。
- `head` 当前默认输出前 `10` 行，并支持 `-n N` 指定输出行数：
  - `run head < /readme.txt`
  - `run head -n 3 < /readme.txt`
- `head` 当前统一通过 `sys_read(0, ...)` 从 stdin 读取，因此可以复用：
  - 文件 stdin 重定向
  - pipe stdin
- `head` 当前通过 `sys_write(...)` 输出前 N 行，因此可以继续走：
  - 屏幕输出
  - `stdout` 重定向写文件
- 当前最小验证链路包括：
  - `run cat /readme.txt | run head`
  - `run cat /readme.txt | run head -n 3`
  - `run cat < /readme.txt | run head -n 3 > /head.txt`
- 当前 `head` 为教学版：
  - 不支持多个文件参数
  - 不支持 GNU `head` 的复杂参数
  - 不支持真实 UNIX pipe / pipe fd / `dup2`

## Task76：用户态 tail 程序 / 简化版尾部输出

- 当前新增了一个最小用户态 `tail` 程序，用来验证 “stdin / pipe -> 用户态缓存尾部 -> stdout” 这条链路。
- `tail` 当前默认输出最后 `10` 行，并支持 `-n N` 指定输出行数：
  - `run tail < /readme.txt`
  - `run tail -n 3 < /readme.txt`
- `tail` 当前统一通过 `sys_read(0, ...)` 从 stdin 读取，因此可以复用：
  - 文件 stdin 重定向
  - pipe stdin
- `tail` 当前在用户态内部维护一个固定窗口，读取完成后再从后往前定位最后 N 行。
- `tail` 当前通过 `sys_write(...)` 输出结果，因此可以继续走：
  - 屏幕输出
  - `stdout` 重定向写文件
- 当前 Phase2 用户态文本工具链已经包括：
  - `cat / wc / grep / head / tail`
- 当前最小验证链路包括：
  - `run cat /readme.txt | run tail`
  - `run cat /readme.txt | run tail -n 3`
  - `run cat < /readme.txt | run tail -n 3 > /tail.txt`
- 当前 `tail` 为教学版：
  - 不支持多个文件参数
  - 不支持 GNU `tail` 的复杂参数
  - 不支持 `-f`
  - 使用固定缓冲区
  - 不支持真实 UNIX pipe / pipe fd / `dup2`

## Task77：用户态 sort 程序 / 小输入行排序

- 当前新增了一个最小用户态 `sort` 程序，用来验证 “stdin / pipe -> 用户态缓存与切行 -> 行排序 -> stdout” 这条链路。
- `sort` 当前按字节字典序对每一行做升序排序：
  - `run sort < /readme.txt`
  - `run cat /readme.txt | run sort`
- `sort` 当前统一通过 `sys_read(0, ...)` 从 stdin 读取，因此可以复用：
  - 文件 stdin 重定向
  - pipe stdin
- `sort` 当前会先把输入读入固定缓冲区，再按 `\n` 切成若干行，最后在用户态内部做简单排序。
- `sort` 当前通过 `sys_write(...)` 输出结果，因此可以继续走：
  - 屏幕输出
  - `stdout` 重定向写文件
- 当前 Phase2 用户态文本工具链已经包括：
  - `cat / wc / grep / head / tail / sort`
- 当前最小验证链路包括：
  - `run sort < /readme.txt > /sorted.txt`
  - `run cat < /readme.txt | run sort > /sorted2.txt`
- 当前 `sort` 为教学版：
  - 不支持多个文件参数
  - 不支持 GNU `sort` 的复杂参数
  - 不支持 `-r`、`-n` 和外部排序
  - 使用固定缓冲区和固定最大行数
  - 不支持真实 UNIX pipe / pipe fd / `dup2`

## Task78：pipe buffer 容量限制与错误处理整理

- 当前任务不新增用户态程序，而是整理教学版顺序 pipe 的边界行为。
- 当前 Phase2 数据流链路可以概括为：
  - 文件系统
  - `-> stdin/stdout`
  - `-> redirect`
  - `-> pipe`
  - `-> 用户态文本工具`
- 当前 pipe 仍然采用顺序模型：
  - shell 先运行左侧程序
  - 左侧程序把 stdout 写入内核 pipe buffer
  - 左侧结束后，shell 再运行右侧程序
  - 右侧从 pipe buffer 读取直到 EOF
- 本轮整理后：
  - pipe buffer 固定容量为 `512` 字节
  - 写入前会检查剩余空间，避免越界
  - 写满时采用“尽量写满剩余空间 + 只提示一次”的教学版策略
  - 读完时统一返回 `0` 表示 EOF
  - 每次执行 pipe 命令前后都会清空 pipe 状态，避免连续命令复用旧数据
- 当前用户态文本工具链仍然是：
  - `cat / wc / grep / head / tail / sort`
- 当前不支持的真实 UNIX pipe 能力包括：
  - pipe fd
  - `dup2`
  - 阻塞读写
  - 并发执行
  - 多级管道
  - 动态扩容

## Task79：真正 pipe fd 雏形

- 当前任务是从“教学版 pipe buffer 特判路径”走向“最小 pipe fd 抽象”的过渡步骤。
- 当前 Phase2 数据流链路可以概括为：
  - 文件系统
  - `-> fd`
  - `-> stdin/stdout`
  - `-> redirect`
  - `-> pipe fd`
  - `-> 用户态文本工具`
- 本轮之后，fd 表已经能区分：
  - 普通文件 fd
  - `pipe read fd`
  - `pipe write fd`
- 当前仍然只保留一个教学版全局 pipe buffer，但 shell 已经会为左右程序绑定最小 pipe fd 端点。
- 当前分发关系是：
  - `SYS_READ(fd, ...)` 读普通文件 fd 或 pipe read fd
  - `SYS_FD_WRITE(fd, ...)` 写普通文件 fd 或 pipe write fd
  - `SYS_WRITE(...)` 在 stdout 被 pipe 接管时，会通过当前进程绑定的 pipe write fd 落到 pipe buffer
- 当前 pipe 仍然是顺序执行，不是并发 UNIX pipe：
  - 左侧先写
  - 右侧后读
  - 共享同一个教学版 pipe buffer
- 当前仍不支持：
  - 用户态 `pipe()`
  - `dup2`
  - fork 后共享 pipe fd
  - 阻塞读写
  - 并发 pipe
  - 多级管道
  - 多个 pipe object

## Task80：fd 抽象整理 / 统一 file fd 与 pipe fd 分发路径

- 当前任务不是新增功能，而是整理 fd 抽象和 read/write 分发路径。
- 当前 Phase2 数据流链路可以概括为：
  - 文件系统
  - `-> fd 抽象`
  - `-> stdin/stdout`
  - `-> redirect`
  - `-> pipe fd`
  - `-> 用户态文本工具`
- 本轮之后，普通文件 fd 与 pipe fd 的查找、分配、清理路径更统一了。
- 当前最小 fd 类型仍然包括：
  - `FD_FILE`
  - `FD_PIPE_READ`
  - `FD_PIPE_WRITE`
- 当前分发关系更清楚了：
  - `SYS_READ(fd, ...)`
    - `fd=0` 先走教学版 stdin 兼容入口
    - `FD_FILE` 读文件
    - `FD_PIPE_READ` 读 pipe
    - `FD_PIPE_WRITE` 返回错误
  - `SYS_FD_WRITE(fd, ...)`
    - `FD_FILE` 写文件
    - `FD_PIPE_WRITE` 写 pipe
    - `FD_PIPE_READ` 返回错误
- 当前仍然保留少量兼容字段与特殊入口：
  - `fd=0`
  - `SYS_WRITE(text)`
  - `stdin_redirect_from_pipe`
  - `stdout_redirect_to_pipe`
- 这些兼容路径暂时保留，是为了不破坏现有 redirect / pipe / RAMFS / 用户态工具链。

## Task81：dup2 雏形 / fd 重定向统一入口

- 当前任务是在 fd 抽象之上继续引入内核内部 `dup2` 雏形。
- 当前 Phase2 数据流链路可以概括为：
  - 文件系统
  - `-> fd 抽象`
  - `-> dup2 雏形`
  - `-> stdin/stdout redirect`
  - `-> pipe fd`
  - `-> 用户态文本工具`
- 本轮不新增用户态 syscall，而是在内核内部提供最小 `fd_dup2(oldfd, newfd)`。
- 当前 `fd_dup2` 的定位是：
  - 统一 shell redirect / pipe 的接线入口
  - 不实现完整 POSIX `dup2`
  - 不实现引用计数或 fork 后共享 fd
- 本轮迁移状态：
  - `pipe`
    - 已开始通过 `fd_dup2(pipe_write_fd, 1)` / `fd_dup2(pipe_read_fd, 0)` 接到标准入口
  - 文件型 stdin/stdout redirect
    - 当前仍保留兼容路径

## Task82：Shell 重定向迁移到 dup2 路径

- 当前任务是 Shell 文件重定向迁移任务，不新增用户态程序。
- 当前 Phase2 数据流链路可以概括为：
  - 文件系统
  - `-> fd 抽象`
  - `-> dup2 雏形`
  - `-> Shell 输入/输出重定向`
  - `-> pipe fd`
  - `-> 用户态文本工具`
- 本轮之后：
  - `run A < input`
    - 已开始优先走 `fd_dup2(input_fd, 0)`
  - `run A > output`
    - 已开始优先走 `fd_dup2(output_fd, 1)`
  - `run A < input > output`
    - 已开始对 `fd=0 / fd=1` 同时做设置
- 当前 pipe 连接由后续 Task83 继续统一。

## Task83：pipe 迁移到 dup2 路径

- 当前任务是把 shell pipe 连接统一到 `fd_dup2` 路径，不新增用户态程序。
- 当前 Phase2 数据流链路可以概括为：
  - 文件系统
  - `-> fd`
  - `-> dup2 雏形`
  - `-> Shell 输入/输出重定向`
  - `-> shell pipe 接线`
  - `-> 用户态文本工具`
- 本轮之后：
  - 左侧 `run A`
    - 已通过 `fd_dup2(pipe_write_fd, 1)` 接到 pipe 写端
  - 右侧 `run B`
    - 已通过 `fd_dup2(pipe_read_fd, 0)` 接到 pipe 读端
  - `run A | run B > output`
    - 右侧仍可继续通过 `fd_dup2(output_fd, 1)` 接到输出文件
  - `run A < input | run B`
    - 左侧仍可继续通过 `fd_dup2(input_fd, 0)` 接到输入文件
- 当前仍然是教学版顺序 pipe，不做并发、不做阻塞、不做多级管道。

## Task84：pipe() syscall 雏形

- 当前任务是把教学版 pipe 从“shell 内部连接”再往前推进一步，新增最小用户态 `pipe()` syscall。
- 当前 Phase2 数据流链路可以概括为：
  - 文件系统
  - `-> fd`
  - `-> dup2 雏形`
  - `-> shell redirect`
  - `-> shell pipe`
  - `-> 用户态 pipe()`
  - `-> 用户态文本工具`
- 本轮之后：
  - 用户程序可以调用 `pipe(fds)`
  - `fds[0]` 返回 `pipe read fd`
  - `fds[1]` 返回 `pipe write fd`
- `pipe_test` 可以直接验证 pipe fd 的最小读写与 EOF 语义
- 当前仍然只支持一个全局教学版 pipe buffer，不支持多个独立 pipe object。

## Task85：dup2 syscall 雏形

- 当前任务是把 `dup2` 从内核内部能力继续暴露给用户态。
- 当前 Phase2 数据流链路可以概括为：
  - `fd 抽象`
  - `-> pipe syscall`
  - `-> dup2 syscall`
  - `-> 后续 fork fd 继承`
  - `-> 用户态组合 pipe`
- 本轮之后：
  - 用户程序可以调用 `dup2(oldfd, newfd)`
  - 成功时返回 `newfd`
  - 失败时返回 `-1`
  - `dup2_test` 可以直接验证 pipe fd 的复制、读写和最小错误路径
- 当前仍然只是教学版 `dup2`，不支持引用计数、close-on-exec 或 fork 后共享 fd。

## Task86：fork 后 fd 继承语义整理

- 当前任务是把 fork 从“只复制用户镜像与返回现场”继续推进到“复制当前教学版 fd 视图”。
- 当前 Phase2 数据流链路可以概括为：
  - `fd 抽象`
  - `-> pipe syscall`
  - `-> dup2 syscall`
  - `-> fork fd 继承`
  - `-> 用户态组合 pipe`
- 本轮之后：
  - 子进程可以继承普通文件 fd
  - 子进程可以继承 pipe read fd / pipe write fd
  - `fork_fd_test` 可以验证父子之间最小 pipe 数据传递
- 当前仍然只是教学版 fd 继承，不支持引用计数或共享 file object。

## Task87：用户态 pipe + fork + dup2 组合测试

- 当前任务是组合验证任务，不新增新的内核主机制。
- 当前 Phase2 数据流链路可以概括为：
  - `fd 抽象`
  - `-> pipe syscall`
  - `-> dup2 syscall`
  - `-> fork fd 继承`
  - `-> 用户态组合 pipe`
- 本轮之后：
  - 用户态程序已经可以自己跑出最小闭环：
    - `pipe()`
    - `fork()`
    - `dup2()`
    - `read/write`
  - `pipe_fork_dup2_test` 可以直接验证这条链路
- 当前仍然没有 `exec`，也不是完整并发阻塞 pipe。

## Task88：pipe read/write 端关闭语义整理

- 当前任务是教学版 pipe 生命周期整理任务，不实现完整 POSIX close 语义。
- 本轮之后：
  - `close(pipe read fd)` 会标记读端关闭
  - `close(pipe write fd)` 会标记写端关闭
  - 写端关闭后，读端读完已有数据返回 EOF
  - 读端关闭后，写端继续写入返回错误或 0，不 panic
- 当前 Phase2 数据流链路可以概括为：
  - `文件系统`
  - `-> fd 抽象`
  - `-> pipe / dup2 / fork`
  - `-> close 语义整理`
  - `-> 用户态组合测试`
- 当前仍然只有一个全局教学版 pipe buffer，也没有引用计数。

## Task89：exec 与 fd 保留语义整理

- 当前任务是“exec 替换镜像，但保留 fd 视图”的语义整理任务。
- 本轮之后：
  - `exec` 默认保留当前进程 fd table
  - `fd=0 / fd=1` 在 exec 后不会被重置
  - 普通文件 fd / pipe fd 在 exec 后仍然有效
- 当前 Phase2 数据流链路可以概括为：
  - `fd 抽象`
  - `-> pipe / dup2 / fork`
  - `-> exec 后 fd 保留`
  - `-> 用户态组合测试`
- 新增 `exec_fd_test` 用来验证：
  - `pipe()`
  - `fork()`
  - `dup2()`
  - `exec()`
  - `read/write`
- 当前仍然不是完整 POSIX exec，也没有 close-on-exec。

## Task90：用户态 pipeline demo / pipe + fork + dup2 + exec 端到端演示

- 当前任务是“把 pipe / fork / dup2 / exec 在用户态自己串起来”的教学版 demo 任务。
- 本轮之后：
  - 新增 `pipeline_demo`
  - 新增 `pipeline_writer`
  - 新增 `pipeline_reader`
- 当前 `pipeline_demo` 采用教学版顺序模型：
  - `pipe(fds)`
  - `fork()` writer 子进程
  - writer 子进程 `dup2(fds[1], 1)` 后 `exec(pipeline_writer)`
  - 父进程 `waitpid(writer)`
  - `fork()` reader 子进程
  - reader 子进程 `dup2(fds[0], 0)` 后 `exec(pipeline_reader)`
  - 父进程 `waitpid(reader)`
  - 最终输出 `pipeline_demo: ok`
- 当前 Phase2 数据流链路可以概括为：
  - `fd 抽象`
  - `-> pipe / dup2 / fork / exec`
  - `-> 用户态 producer / consumer demo`
- 当前仍然不是完整 UNIX pipeline：
  - 仍然只有一个全局教学版 pipe buffer
  - 没有并发阻塞 pipe
  - 不支持多级管道

## Task91：exec 参数传递整理 / argv 语义补齐

- 当前任务是“把教学版 exec 的 argc / argv 路径讲清楚，并新增最小验证程序”的参数语义整理任务。
- 本轮之后：
  - 新增 `exec_args_test`
  - 新增 `exec_args_target`
  - 明确 `SYS_EXEC_ARGS` 的最小参数模型：
    - `argc`
    - `argv[0]`
    - 普通字符串参数
- 当前 Phase2 数据流链路可以概括为：
  - `shell argv`
  - `-> SYS_EXEC_ARGS`
  - `-> PCB 暂存参数`
  - `-> 新程序通过 SYS_GET_ARGC / SYS_GET_ARG 读取`
- 当前仍然不是完整 POSIX `execve`：
  - 没有 `envp`
  - 没有完整用户栈 `argc/argv` ABI
  - 不支持复杂引号与转义

## Task92：用户态 pipeline demo 支持带参数程序 / argv + pipe 组合验证

- 当前任务是“把 exec 参数传递和 pipe/fork/dup2 组合起来”的用户态 demo 验证任务。
- 本轮之后：
  - 新增 `pipeline_args_demo`
  - 验证带参数程序可以作为 pipeline 右侧 consumer 运行
- 当前 `pipeline_args_demo` 采用教学版顺序模型：
  - writer 子进程 `exec(pipeline_writer)`
  - consumer 子进程 `exec(grep, argv={"grep","MiniOS"})`
- 当前 Phase2 数据流链路可以概括为：
  - `pipe`
  - `-> fork`
  - `-> dup2`
  - `-> exec(argc, argv)`
  - `-> 带参数 consumer`
- 当前仍然不是完整 UNIX pipeline：
  - 仍然只有一个全局教学版 pipe buffer
  - 没有并发阻塞 pipe
  - 不支持多级管道

## Task93：Shell argv parser 整理 / 为 mini_pipeline 命令做准备

- 当前任务不是重写整个 shell，而是整理 `run` 命令在 `argv / redirect / pipe` 交织场景下的参数边界。
- 本轮之后，普通 `run` 和 pipe 左右两侧都会统一按“先识别 `< / > / >>`，再计算真正用户程序 `argc`”的规则工作。
- 当前可以更稳定地支持：
  - `run grep MiniOS < /readme.txt`
  - `run head -n 3 < /readme.txt`
  - `run tail -n 3 < /readme.txt`
  - `run cat /readme.txt | run grep MiniOS`
  - `run cat /readme.txt | run head -n 3`
- 当前 Phase2 数据流链路可以概括为：
  - `shell token`
  - `-> run argv`
  - `-> redirect / pipe 剥离`
  - `-> 用户程序 argc/argv`
- 当前仍然只支持最小 shell 语法：
  - 不支持引号
  - 不支持转义
  - 不支持多级管道

## Task94：mini_pipeline 命令 / 用户态固定管道命令入口

- 当前任务不是重写 shell，也不是直接扩展真正 `|` 语法，而是新增一个用户态固定格式命令入口：
  - `run mini_pipeline <left_prog> -- <right_prog> [right_args...]`
- 本轮之后，MiniOS 已经具备一个可直接演示的用户态 pipeline 命令：
  - 左侧程序名
  - `--`
  - 右侧程序名与参数
- 当前 `mini_pipeline` 采用教学版顺序模型：
  - `pipe(fds)`
  - `fork()` 左侧 writer
  - 左侧 `dup2(fds[1], 1)` 后 `exec(left_prog)`
  - 父进程 `waitpid(writer)`
  - `fork()` 右侧 consumer
  - 右侧 `dup2(fds[0], 0)` 后 `exec(right_prog, argv)`
  - 父进程 `waitpid(consumer)`
- 当前 Phase2 数据流链路可以概括为：
  - `shell argv`
  - `-> mini_pipeline argv`
  - `-> pipe`
  - `-> fork`
  - `-> dup2`
  - `-> exec(argc, argv)`
  - `-> 固定 pipeline 命令入口`
- 当前仍然不是完整 UNIX pipeline：
  - 不支持多级管道
  - 左侧暂不支持参数
  - 仍然只有一个全局教学版 pipe buffer
