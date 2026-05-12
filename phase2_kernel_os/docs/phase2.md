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
