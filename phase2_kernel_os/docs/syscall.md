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
| 11 | `SYS_EXEC_ARGS` | `user_exec_args` | `ebx=program_id` `ecx=argc` `edx=argv` | 成功后不回到旧镜像，失败返回负值 | 当前 shell 与 `exec_args_test` 主要使用的 exec 入口 |
| 12 | `SYS_PS` | `user_ps_get` | `ebx=index` `ecx=process_info*` | 成功返回 `0`，越界/失败返回负值 | 逐条读取进程摘要 |
| 13 | `SYS_KILL` | `user_kill` | `ebx=pid` | 成功返回 `0`，失败返回负值 | 教学版按 pid 终止目标进程 |
| 14 | `SYS_WAIT_ANY` | 当前无统一 shell 封装 | 无 | 回收成功返回 pid，无可回收返回 `0`，失败返回负值 | 非阻塞回收任意 zombie 子进程 |
| 15 | `SYS_YIELD` | 当前无统一 shell 封装 | 无 | 通常返回 `0` / 负值 | 主动让出 CPU |
| 16 | `SYS_SLEEP` | `user_sleep_ticks` | `ebx=ticks` | 成功返回 `0`，失败返回负值 | 当前进程睡眠若干 tick |
| 17 | `SYS_SLEEP_PID` | `user_sleep_pid` | `ebx=pid` `ecx=ticks` | 成功返回 `0`，失败返回负值 | 教学调试接口 |
| 18 | `SYS_SET_BACKGROUND` | `user_set_background` | `ebx=pid` `ecx=flag` | 成功返回 `0`，失败返回负值 | 给后台任务打标记 |
| 19 | `SYS_GET_TICKS` | `user_get_ticks` | 无 | 当前系统 tick 数 | 当前 `uptime/ticks` 命令主要使用的时间接口 |
| 20 | `SYS_CLEAR_SCREEN` | `user_clear_screen` | 无 | `0` / 负值 | 清空 VGA 文本屏幕 |
| 21 | `SYS_OPEN` | `user_open` | `ebx=path` | `fd` 或负值 | 以只读方式打开一个当前可见文件 |
| 22 | `SYS_READ` | `user_read` | `ebx=fd` `ecx=buf` `edx=size` | 字节数 / `0` / 负值 | 从 fd 当前 offset 读取数据 |
| 23 | `SYS_CLOSE` | `user_close` | `ebx=fd` | `0` 或负值 | 关闭一个已打开 fd |
| 24 | `SYS_FILE_COUNT` | `user_file_count` | 无 | 文件数量 / 负值 | 返回当前可见文件数量 |
| 25 | `SYS_FILE_INFO` | `user_file_info` | `ebx=index` `ecx=buf` `edx=max_len` | 文件大小 / 负值 | 复制指定索引文件路径并返回大小 |
| 26 | `SYS_STAT` | `user_stat` | `ebx=path` `ecx=minios_stat*` | `0` / 负值 | 查询单个文件的教学版元信息 |
| 27 | `SYS_TOUCH` | `user_touch` | `ebx=path` | `0` / 负值 | 创建空 RAMFS 文件 |
| 28 | `SYS_WRITEFILE` | `user_writefile` | `ebx=path` `ecx=text` | `0` / 负值 | 覆盖写入 RAMFS 文本文件 |
| 29 | `SYS_RM` | `user_rm` | `ebx=path` | `0` / 负值 | 删除一个 RAMFS 文件 |
| 30 | `SYS_OPEN_WRITE` | `user_open_write` | `ebx=path` | `fd` / 负值 | 以可写方式打开一个已存在的 RAMFS 文件 |
| 31 | `SYS_FD_WRITE` | `user_fd_write` | `ebx=fd` `ecx=buf` `edx=size` | 字节数 / 负值 | 通过 fd 向 RAMFS 文件写入文本内容 |
| 32 | `SYS_APPEND_FILE` | `user_append_file` | `ebx=path` `ecx=text` | 追加字节数 / 负值 | 向 RAMFS 文件末尾追加文本内容 |
| 40 | `SYS_PIPE` | `user_pipe` / `pipe_test` 内部包装 | `ebx=int fds[2]` | `0` / 负值 | 为当前进程创建一对教学版 pipe fd |
| 41 | `SYS_DUP2` | `user_dup2` / `dup2_test` 内部包装 | `ebx=oldfd` `ecx=newfd` | `newfd` / `-1` | 把 oldfd 复制或绑定到 newfd |

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

## 5. SYS_EXEC_ARGS 当前参数模型

当前 `SYS_EXEC_ARGS` 是教学版最小 exec 参数入口：

1. `ebx = program_id`
2. `ecx = argc`
3. `edx = argv`

当前支持：

1. `argc`
2. `argv[0]`
3. 普通字符串参数
4. 固定参数数量上限
5. 固定参数长度上限

当前限制：

1. 不支持 `envp`
2. 不支持完整 POSIX `execve`
3. 不支持复杂用户栈参数布局
4. 不支持引号和转义解析

当前参数约束来自统一常量：

1. 最大参数数量：`USER_PROGRAM_MAX_ARGS = 10`
2. 单个参数最大长度：`USER_PROGRAM_MAX_ARG_LEN = 32`

失败语义：

1. `argc` 越界时返回负值
2. `argv` 为 `NULL` 且 `argc > 0` 时返回负值
3. 任一参数字符串过长时返回负值
4. 参数复制失败时不会 panic
5. exec 失败时不会主动清空 fd table

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
3. 当前只实现教学版最小 fd 表
4. 当前只实现教学版 `open/read/close`，仍无真实磁盘文件接口
5. `exec` 仍基于内置 `program_id`
6. `argv` 仍是教学版实现，参数暂存在 PCB，而不是真正按完整用户栈 ABI 组织
7. 部分 syscall 仍带有教学调试性质，例如 `sleep_pid`
8. Phase3 若引入更真实文件系统和用户程序加载，syscall ABI 仍可能继续演进

## 7. SYS_OPEN / SYS_READ / SYS_CLOSE 当前语义

当前文件 syscall 只服务于内置只读文本文件：

1. `SYS_OPEN(path)`：成功返回 fd，失败返回负值
2. fd 从 `3` 开始分配
3. `SYS_READ(fd, buf, size)`：成功返回读取字节数，EOF 返回 `0`
4. `SYS_CLOSE(fd)`：成功返回 `0`
5. 当前 `cat <file>` 已通过这套 fd 层读取文件

它不是完整 Unix 文件系统接口，仍不支持写入、目录树、inode、block cache 或真实磁盘。

当前用户态 `cat` 也已经通过这组 syscall 工作：

```text
run cat /readme.txt
    -> SYS_OPEN(path)
    -> SYS_READ(fd, buf, size)
    -> SYS_WRITE(buf)
    -> SYS_CLOSE(fd)
```

用户指针限制：

1. `SYS_OPEN` 需要从用户态读取路径字符串
2. `SYS_READ` 需要把数据写回用户缓冲区
3. 当前仍依赖教学版共享映射和最小长度约束
4. 暂无完整用户指针合法性校验与隔离防护

## 8. SYS_FILE_COUNT / SYS_FILE_INFO 当前语义

当前文件列表 syscall 仍然只服务于内置只读文件表，不是完整目录系统：

1. `SYS_FILE_COUNT()`：返回当前内置文件数量
2. `SYS_FILE_INFO(index, buf, max_len)`：把指定索引的文件路径复制到 `buf`
3. `SYS_FILE_INFO` 成功时返回该文件大小
4. `index` 越界、`buf` 为空或缓冲区太小时返回负值

当前用户态 `ls` 已通过这组 syscall 工作：

```text
run ls
    -> SYS_FILE_COUNT()
    -> SYS_FILE_INFO(index, buf, max_len)
    -> SYS_WRITE(buf)
```

它只是把“列出内置只读文件元信息”暴露给用户态程序，还不是 `readdir/getdents` 一类真实目录接口。

## 9. SYS_STAT 当前语义

当前 `SYS_STAT` 是教学版单文件元信息查询接口：

1. `SYS_STAT(path, stat_buf)`：成功返回 `0`
2. `path` 是用户态路径字符串
3. `stat_buf` 是用户态 `struct minios_stat*`
4. 当前只写入：
   - `size`
   - `type`
5. 文件不存在、参数为空或路径非法时返回负值

当前用户态 `stat` 已通过这组语义工作：

```text
run stat /readme.txt
    -> SYS_STAT(path, stat_buf)
    -> SYS_WRITE("Name/Size/Type")
```

当前 `struct minios_stat` 不是 POSIX `struct stat`，不包含 inode、权限、uid/gid、时间戳或 block 数等字段。

当前 `type` 最小支持：

1. `readonly-file`
2. `ramfs-file`

## 10. SYS_TOUCH / SYS_WRITEFILE / SYS_RM 当前语义

Task62 继续把“可写内存文件”暴露到 shell 用户体验中，但当前仍保持最小教学版接口：

1. `SYS_TOUCH(path)`：创建空 RAMFS 文件
2. `SYS_WRITEFILE(path, text)`：覆盖写入一个 RAMFS 文本文件
3. `SYS_RM(path)`：删除一个 RAMFS 文件

当前规则：

1. 内置只读文件不能被 `SYS_WRITEFILE` 或 `SYS_RM`
2. `SYS_WRITEFILE` 当前要求文件已存在，推荐先 `touch`
3. RAMFS 文件内容超过上限时直接失败
4. 这组接口当前主要服务于 shell 内建命令，尚未扩展成 `write(fd)` 语义

## 11. SYS_OPEN_WRITE / SYS_FD_WRITE 当前语义

Task63 继续把 RAMFS 写入能力暴露给用户态程序，但保持最小教学版设计。

1. `SYS_OPEN_WRITE(path)`：
   - 文件必须已存在
   - 文件必须是 RAMFS 文件
   - 成功返回一个可写 fd
   - 如果目标是内置只读文件或不存在，则返回负值
2. `SYS_FD_WRITE(fd, buf, size)`：
   - 只接受通过 `SYS_OPEN_WRITE` 打开的 RAMFS 文件 fd
   - 成功返回实际写入字节数
   - 当前采用覆盖写语义
   - 写入超过 RAMFS 文件大小上限时失败

当前 `SYS_WRITE` 仍然保持原来的 stdout 输出语义，没有被改成通用 `write(fd, buf, size)`。这是为了不破坏已有 `user_write` / shell 输出 ABI。

## 12. SYS_APPEND_FILE 当前语义

Task64 继续给 RAMFS 补充教学版 append 接口。

1. `SYS_APPEND_FILE(path, text)`：
   - `ebx=path`
   - `ecx=text`
2. 成功时返回实际追加字节数
3. 失败时返回负值
4. 只允许追加到已存在的 RAMFS 文件
5. 内置只读文件不能 append
6. 文件不存在时失败，推荐先 `touch`
7. 当前不自动补空格，也不自动补换行
8. 追加后如果超过 `MAX_RAMFS_FILE_SIZE`，则失败且不破坏原内容

当前它不是完整 POSIX `O_APPEND`，也不保证并发原子追加，只是教学版最小 append syscall。

## 13. SYS_PIPE 当前语义

Task84 新增了教学版最小 `pipe()` syscall：

1. 用户态传入 `int fds[2]`
2. 内核为当前进程分配：
   - `fds[0] = pipe read fd`
   - `fds[1] = pipe write fd`
3. 成功返回 `0`
4. 失败返回负值

当前限制：

1. 仍然只复用一个全局教学版 pipe buffer
2. 不支持多个独立 pipe object
3. 不支持阻塞读写
4. 不支持并发 pipe
5. 不支持用户态 `dup2()`

最小错误保护：

1. `fds == NULL` 时返回失败
2. `fds` 明显不在当前进程用户页/用户栈映射范围内时返回失败
3. fd 分配失败时返回失败

当前最小使用方式：

```text
pipe(fds)
write(fds[1], ...)
read(fds[0], ...)
```

## 14. SYS_DUP2 当前语义

当前教学版 `dup2(oldfd, newfd)` 的最小语义是：

1. 内部复用内核既有 `fd_dup2`
2. `oldfd` 必须是一个当前已打开的教学版 fd
3. `newfd` 当前支持：
   - `0`
   - `1`
   - `>= 3` 的普通教学版 fd 编号
4. 成功返回 `newfd`
5. 失败统一返回 `-1`
6. `oldfd == newfd` 时稳定返回 `newfd`

当前限制：

1. 不是完整 POSIX `dup2`
2. 不支持引用计数
3. `newfd >= 3` 时仍然是教学版表项复制，不共享同一个 file object
4. 不支持 fork 后共享 fd
5. 不支持 close-on-exec

## 15. Task65 与 syscall 的关系

Task65 新增的是 shell 语法层的 `>` / `>>`，本轮没有再新增新的 syscall 编号。

当前实现方式是：

1. `echo text > /file`
   - shell 直接复用已有 `SYS_TOUCH` / `SYS_WRITEFILE`
2. `echo text >> /file`
   - shell 直接复用已有 `SYS_APPEND_FILE`

因此 Task65 还不是完整的“stdout fd 重定向”实现，而是教学版最小 shell 重定向：

- 不改写通用 stdout
- 不引入 `dup2`
- 不支持把任意用户程序输出重定向到文件

## 14. SYS_SET_STDOUT_REDIRECT 与 Task66 的关系

Task66 继续把 shell 重定向推进到用户态程序，但当前仍保持教学版最小接口。

新增：

1. `SYS_SET_STDOUT_REDIRECT(path, append)`
   - `ebx=path`
   - `ecx=append`
   - 作用对象是“当前进程”
   - 成功返回 `0`
   - 失败返回负值

当前 shell 在 `run ... > file` / `run ... >> file` 时，会在子进程 `exec` 前先调用该接口，把 stdout 重定向配置写进当前 PCB。

之后用户程序继续正常调用：

1. `SYS_WRITE(text)`

内核在处理 `SYS_WRITE` 时会检查：

1. 当前进程是否启用了 stdout 重定向
2. 如果没有启用，则仍输出到屏幕
3. 如果启用了，则把文本写到 RAMFS 文件

当前这不是完整 `dup2` / fd 复制模型，用户程序也看不到新的 fd，只是教学版“按进程记录 stdout 去向”。

## 15. SYS_SET_STDIN_REDIRECT 与 Task67 的关系

Task67 当前新增的是教学版 stdin 重定向配置接口。

新增：

1. `SYS_SET_STDIN_REDIRECT(pid, path)`
   - `ebx=pid`
   - `ecx=path`
   - shell 在 `run ... < file` 时调用它，为即将 exec 的子进程设置 stdin 来源

当前 `SYS_READ` 对 `fd=0` 的教学版行为是：

1. 如果当前进程启用了 stdin 重定向
   - `SYS_READ(0, buf, size)` 从 `stdin_redirect_path` 指向的文件读取
   - 每次读取后推进 `stdin_redirect_offset`
   - 到 EOF 返回 `0`
2. 如果当前进程没有启用 stdin 重定向
   - `SYS_READ(0, ...)` 当前直接返回 `0`
   - 不尝试做真实 tty/键盘 stdin
3. 如果 `fd >= 3`
   - 保持 Task58 的原有 fd 读取逻辑

这意味着用户态程序不需要知道“自己是不是被 `< file` 启动的”，它只需要正常调用：

```text
sys_read(0, buf, size)
```

当前这仍不是完整 `dup2` / fd 复制 / tty 模型，只是教学版“按进程记录 stdin 来源”。

## 16. Task68 组合重定向下的 SYS_READ / SYS_WRITE

Task68 不新增新的重定向 syscall，而是把 Task66 / Task67 已有两条链路同时启用。

例如：

```text
run cat < /readme.txt > /copy.txt
```

当前行为是：

1. shell 在创建子进程后：
   - 调用 `SYS_SET_STDIN_REDIRECT(pid, "/readme.txt")`
   - 调用 `SYS_SET_STDOUT_REDIRECT(pid, "/copy.txt", 0)`
2. 用户程序 `cat` 本身不需要知道自己被重定向
3. `cat` 调用：
   - `sys_read(0, buf, size)` 时，内核从输入文件读取
   - `sys_write(text)` 时，内核把输出写到目标 RAMFS 文件

如果使用：

```text
run cat < /input.txt >> /log.txt
```

则 `SYS_WRITE` 继续按追加语义工作。

因此，Task68 的重点不是新增 syscall，而是让：

1. `SYS_READ(fd=0)`
2. `SYS_WRITE`

在同一个进程中同时根据各自的重定向字段独立工作。

## 17. Task69：pipe buffer 下的 SYS_READ / SYS_WRITE

Task69 当前新增的是教学版单管道相关 syscall：

1. `SYS_PIPE_RESET`
   - 清空全局 pipe buffer
2. `SYS_SET_STDOUT_PIPE(pid)`
   - 把指定子进程 stdout 改为写入 pipe buffer
3. `SYS_SET_STDIN_PIPE(pid)`
   - 把指定子进程 stdin 改为从 pipe buffer 读取

### SYS_WRITE 当前顺序

当前 `SYS_WRITE` 的分流顺序是：

1. 若当前进程启用了 `stdout -> pipe`
   - 文本写入 pipe buffer
   - 不输出到屏幕
2. 否则若当前进程启用了 `stdout -> RAMFS`
   - 走 Task66 文件重定向
3. 否则
   - 正常输出到屏幕

### SYS_READ 当前顺序

当前 `SYS_READ` 的 `fd=0` 行为是：

1. 若当前进程启用了 `stdin <- pipe`
   - 从 pipe buffer 读取
   - 读到 EOF 返回 `0`
2. 否则若启用了 `stdin <- file`
   - 走 Task67 文件输入重定向
3. 否则
   - 当前返回 `0`

### 当前限制

1. 当前不是 pipe fd
2. 当前不是并发 pipe
3. 当前不支持阻塞读写
4. 当前不支持 dup2

## 18. Task70：pipe + output redirect 下的 SYS_READ / SYS_WRITE

Task70 没有新增新的 pipe syscall，而是把 Task69 与 Task66 的已有行为组合起来。

支持示例：

```text
run cat /readme.txt | run cat > /copy.txt
run cat /programs | run cat >> /log.txt
```

### 当前配合方式

左侧进程：

1. 启用 `stdout -> pipe`
2. `SYS_WRITE` 继续把文本写入 pipe buffer

右侧进程：

1. 启用 `stdin <- pipe`
2. 启用 `stdout -> RAMFS file`
3. `SYS_READ(fd=0)` 从 pipe buffer 读取
4. `SYS_WRITE` 再把输出写入目标 RAMFS 文件

### 当前顺序

1. 左侧 `SYS_WRITE`
   - pipe 优先
2. 右侧 `SYS_READ(fd=0)`
   - pipe 优先于文件 stdin
3. 右侧 `SYS_WRITE`
   - 因为右侧没有启用 `stdout -> pipe`
   - 所以继续走 Task66 的 `stdout -> file`

### 说明

1. 用户程序本身不需要知道自己被组合重定向
2. 当前仍不是完整 `dup2` / pipe fd 模型
3. 当前右侧既可以从 pipe 读，也可以向 RAMFS 文件写

## 19. Task71：pipe + input redirect 下的 SYS_READ / SYS_WRITE

Task71 没有新增新的 syscall，而是把 Task67 的 `stdin <- file` 和 Task69 的 `stdout -> pipe` 组合起来。

支持示例：

```text
run cat < /readme.txt | run cat
run cat < /programs | run cat
run cat < /input.txt | run cat
```

### 当前配合方式

左侧进程：

1. 启用 `stdin <- file`
2. 启用 `stdout -> pipe`
3. `SYS_READ(fd=0)` 从输入文件读取
4. `SYS_WRITE` 把输出写入 pipe buffer

右侧进程：

1. 启用 `stdin <- pipe`
2. `SYS_READ(fd=0)` 从 pipe buffer 读取
3. `SYS_WRITE` 正常输出到屏幕

### 当前顺序

1. 左侧 `SYS_READ(fd=0)`
   - 文件 stdin 优先
2. 左侧 `SYS_WRITE`
   - pipe 优先
3. 右侧 `SYS_READ(fd=0)`
   - pipe 优先
4. 右侧 `SYS_WRITE`
   - 因为没有启用 `stdout -> pipe` 或 `stdout -> file`
   - 所以继续正常输出到屏幕

### 说明

1. 当前仍不是完整 `dup2` / pipe fd 模型
2. 当前不需要用户程序感知“自己在管道左侧还是右侧”
3. 只要程序用 `sys_read(0, ...)` 和 `sys_write(...)`，内核就会按进程标记决定数据流向

## 20. Task72：完整单管道数据流下的 SYS_READ / SYS_WRITE

Task72 没有新增新的 syscall，而是把 Task71 的“左侧文件 stdin + pipe stdout”和 Task70 的“右侧 pipe stdin + 文件 stdout”组合起来。

支持示例：

```text
run cat < /readme.txt | run cat > /copy.txt
run cat < /programs | run cat > /programs_copy.txt
run cat < /input.txt | run cat >> /log.txt
```

### 当前配合方式

左侧进程：

1. 启用 `stdin <- file`
2. 启用 `stdout -> pipe`
3. `SYS_READ(fd=0)` 从输入文件读取
4. `SYS_WRITE` 把输出写入 pipe buffer

右侧进程：

1. 启用 `stdin <- pipe`
2. 启用 `stdout -> RAMFS file`
3. `SYS_READ(fd=0)` 从 pipe buffer 读取
4. `SYS_WRITE` 把输出写入目标 RAMFS 文件

### 当前顺序

1. 左侧 `SYS_READ(fd=0)`
   - 文件 stdin 生效
2. 左侧 `SYS_WRITE`
   - pipe 优先
3. 右侧 `SYS_READ(fd=0)`
   - pipe 优先
4. 右侧 `SYS_WRITE`
   - 因为右侧没有启用 `stdout -> pipe`
   - 所以继续走 Task66 的 `stdout -> file`

### 说明

1. 当前仍不是完整 `dup2` / pipe fd 模型
2. 用户程序本身不需要知道自己处于“文件输入 + 管道 + 文件输出”的组合链路
3. 只要左侧按 stdin 模式读取、右侧按 stdin 模式读取，内核就会按进程标记自动完成数据转发

## 21. Task73：wc 程序依赖的 syscall 数据流

Task73 没有新增 syscall，而是用新的用户态 `wc` 程序验证已有 syscall 组合已经能承载“读入数据 -> 用户态处理 -> 输出结果”。

`wc` 当前主要依赖：

1. `SYS_READ(fd=0)`
2. `SYS_WRITE`
3. `SYS_EXIT`

### 当前读取来源

1. `run wc < /readme.txt`
   - `SYS_READ(fd=0)` 来自文件 stdin 重定向
2. `run cat /readme.txt | run wc`
   - `SYS_READ(fd=0)` 来自 pipe buffer
3. `run cat < /input.txt | run wc > /count.txt`
   - `SYS_READ(fd=0)` 来自 pipe buffer
   - `SYS_WRITE` 写入 RAMFS 文件

### 说明

1. `wc` 当前不直接打开路径
2. `wc` 统一把 stdin 当作输入源
3. `fd=0` 当前仍然来自文件 stdin 重定向或 pipe，不是交互式 tty

## 22. Task74：grep 程序依赖的 syscall 数据流

Task74 没有新增 syscall，而是用新的用户态 `grep` 程序验证已有 syscall 组合已经能承载“读入文本 -> 用户态过滤 -> 输出匹配行”。

`grep` 当前主要依赖：

1. `SYS_READ(fd=0)`
2. `SYS_WRITE`
3. `SYS_EXIT`
4. `SYS_GET_ARGC`
5. `SYS_GET_ARG`

### 当前读取来源

1. `run grep MiniOS < /readme.txt`
   - `SYS_READ(fd=0)` 来自文件 stdin 重定向
2. `run cat /readme.txt | run grep MiniOS`
   - `SYS_READ(fd=0)` 来自 pipe buffer
3. `run cat < /readme.txt | run grep MiniOS > /grep.txt`
   - `SYS_READ(fd=0)` 来自 pipe buffer
   - `SYS_WRITE` 写入 RAMFS 文件

### 说明

1. `grep` 当前不直接打开路径
2. `grep` 第一个参数只作为关键字
3. `fd=0` 当前仍然来自文件 stdin 重定向或 pipe，不是交互式 tty

## 23. 交叉阅读

如果想从系统调用继续追主线，建议配合：

1. [phase2_summary.md](/home/wyh/MiniOS/phase2_kernel_os/docs/phase2_summary.md)
2. [fd.md](/home/wyh/MiniOS/phase2_kernel_os/docs/fd.md)
3. [process.md](/home/wyh/MiniOS/phase2_kernel_os/docs/process.md)
