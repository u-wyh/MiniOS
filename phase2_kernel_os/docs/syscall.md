# MiniOS Phase2 系统调用表

## 1. syscall 调用约定

当前 MiniOS Phase2 的系统调用仍是教学版最小 ABI：

1. 用户态通过 `int 0x80` 进入内核。
2. `eax` 放 syscall 编号。
3. `ebx`、`ecx`、`edx` 依次放前 1 到 3 个参数。
4. 返回值放回 `eax`。
5. 当前阶段不支持 `errno`，因此失败通常直接返回负值。

这套约定的重点是简单、稳定、易观察，而不是追求 Linux 级完整接口。

## 2. syscall 总表

| 编号 | 名称 | 用户态封装 | 参数 | 返回值 | 说明 |
|---|---|---|---|---|---|
| 1 | `SYS_WRITE` | `user_write` | `ebx=text` | `0` / 负值 | 向控制台输出字符串 |
| 2 | `SYS_EXIT` | `user_exit` | `ebx=status` | 通常不返回 | 结束当前进程 |
| 3 | `SYS_GETPID` | 当前无统一 shell 封装 | 无 | 当前 pid | 历史教学接口 |
| 4 | `SYS_TIME` | 当前无统一 shell 封装 | 无 | 当前 tick | 历史教学接口，语义接近 `get_ticks` |
| 5 | `SYS_FORK` | `user_fork` | 无 | 父返回 `child_pid`，子返回 `0` | 复制当前进程 |
| 6 | `SYS_WAITPID` | `user_waitpid` | `ebx=pid` | 成功返回 pid，失败返回负值 | 等待并回收指定子进程 |
| 7 | `SYS_EXEC` | 当前无统一 shell 封装 | `ebx=program_id` | 成功后不回到旧镜像，失败返回负值 | 历史最小 exec 接口 |
| 8 | `SYS_READ_CHAR` | `user_read_char` | 无 | ASCII / 阻塞后恢复 | 读取一个字符 |
| 9 | `SYS_GET_ARGC` | 用户程序内部原始调用 | 无 | `argc` | 读取当前进程保存的教学版参数个数 |
| 10 | `SYS_GET_ARG` | 用户程序内部原始调用 | `ebx=index` `ecx=buf` `edx=max_len` | 成功返回长度，失败返回负值 | 读取一项教学版 argv |
| 11 | `SYS_EXEC_ARGS` | `user_exec_args` | `ebx=program_id` `ecx=argc` `edx=argv` | 成功后不回到旧镜像，失败返回负值 | 当前 shell 主要使用的 exec 入口 |
| 12 | `SYS_PS` | `user_ps_get` | `ebx=index` `ecx=process_info*` | 成功返回 `0`，越界/失败返回负值 | 逐条读取进程摘要 |
| 13 | `SYS_KILL` | `user_kill` | `ebx=pid` | 成功返回 `0`，失败返回负值 | 教学版按 pid 终止目标进程 |
| 14 | `SYS_WAIT_ANY` | 当前无统一 shell 封装 | 无 | 回收成功返回 pid，无可回收返回 `0`，失败返回负值 | 非阻塞回收任意 zombie 子进程 |
| 15 | `SYS_YIELD` | 当前无统一 shell 封装 | 无 | 通常返回 `0` / 负值 | 主动让出 CPU |
| 16 | `SYS_SLEEP` | `user_sleep_ticks` | `ebx=ticks` | 成功返回 `0`，失败返回负值 | 当前进程睡眠若干 tick |
| 17 | `SYS_SLEEP_PID` | `user_sleep_pid` | `ebx=pid` `ecx=ticks` | 成功返回 `0`，失败返回负值 | 教学调试接口 |
| 18 | `SYS_SET_BACKGROUND` | `user_set_background` | `ebx=pid` `ecx=flag` | 成功返回 `0`，失败返回负值 | 给后台任务打标记 |
| 19 | `SYS_GET_TICKS` | `user_get_ticks` | 无 | 当前系统 tick 数 | 当前 `uptime/ticks` 命令主要使用的时间接口 |
| 20 | `SYS_CLEAR_SCREEN` | `user_clear_screen` | 无 | `0` / 负值 | 清空 VGA 文本屏幕 |

说明：

1. “当前无统一 shell 封装”不代表 syscall 不可用，只表示当前仓库里没有专门抽成一份公共用户态包装函数。
2. 某些用户程序 ELF 直接在自己的最小源码里用原始 `int 0x80`，这仍然符合当前教学版 ABI。

## 3. 返回值约定

当前阶段的返回值约定以“最小可用”优先：

1. 普通成功：很多 syscall 成功时返回 `0`
2. 普通失败：当前通常直接返回负值，不通过 `errno` 细分
3. `fork`：父进程返回 `child_pid`，子进程恢复后返回 `0`
4. `waitpid`：成功返回被等待/回收的 pid，失败返回负值
5. `wait_any`：成功回收返回 pid，无可回收 zombie 返回 `0`
6. `read_char`：读到字符返回 ASCII；若当前没有输入，可能先阻塞再恢复
7. `get_ticks` / `time`：直接返回当前 tick 数
8. `exec` / `exec_args`：成功后当前进程镜像被替换，因此不会按“旧程序继续执行”的方式返回成功值
9. `kill`：成功返回 `0`；目标不存在、目标是 init、目标是当前 shell、目标已经退出等情况返回负值

因此，MiniOS 当前 syscall ABI 不是“所有接口都严格统一成 0/-1”，而是保留了少量教学上更直观的特殊返回。

## 4. SYS_KILL 当前语义

`SYS_KILL` 是当前 MiniOS 的教学版进程控制接口：

1. 用户态把目标 `pid` 放入 `ebx`
2. 内核调用 `process_kill(pid, PROCESS_KILL_EXIT_STATUS)`
3. 可终止目标进入 `PROCESS_ZOMBIE`
4. 调度器不再选择该目标运行
5. 父进程通过 `wait/waitpid/wait_any` 回收，孤儿进程由 init/reaper 兜底回收

当前 `PROCESS_KILL_EXIT_STATUS` 只表示“该进程被 kill 终止”，不是 Unix/Linux 的 `SIGKILL` 编号。MiniOS 仍不支持 `kill -9`、信号处理函数、进程组 kill 或权限检查。

## 5. SYS_SLEEP / SYS_GET_TICKS 当前语义

当前时间相关 syscall 采用教学版最小模型：

1. `SYS_GET_TICKS` 直接返回系统启动以来累计的 PIT tick 数
2. 当前 PIT 默认频率为 `20Hz`
3. 因此 `1 tick ≈ 50ms`
4. `SYS_SLEEP(ticks)` 的参数单位就是 tick，而不是秒
5. `sleep(0)` 当前退化为最小 `yield`，不会把进程永久挂起

内核内部仍统一使用 tick 做调度统计、睡眠唤醒和 uptime 展示，不引入 RTC、时区或 wall clock 语义。

## 6. 当前限制

当前 syscall ABI 仍有这些明确限制：

1. 暂无 `errno`
2. 暂无完整用户指针校验
3. 暂无文件描述符表
4. 暂无 `open/read/write` 文件接口
5. `exec` 仍基于内置 `program_id`
6. `argv` 仍是教学版实现，参数暂存在 PCB，而不是真正按完整用户栈 ABI 组织
7. 部分 syscall 仍带有教学调试性质，例如 `sleep_pid`
8. Phase3 若引入更真实文件系统和用户程序加载，syscall ABI 仍可能继续演进
