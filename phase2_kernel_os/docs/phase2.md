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
