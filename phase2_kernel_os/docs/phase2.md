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
