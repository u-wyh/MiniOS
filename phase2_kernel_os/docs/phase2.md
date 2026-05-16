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
