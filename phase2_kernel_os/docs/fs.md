# MiniOS Phase2 文件系统雏形

## 1. 当前定位

当前 MiniOS 还没有真实磁盘文件系统。

本阶段的“文件”来自内核静态只读数据，主要用于让 shell 先具备最小的：

- `ls`
- `cat <file>`

语义。

## 2. 内置只读文件表

当前内核维护一张统一的内置只读文本文件表。

每个文件至少包含：

- `path`
- `content`
- `size`

当前内置文件包括：

- `/readme.txt`
- `/programs`
- `/help.txt`

## 3. ls / cat

### ls

`ls` 当前只列出内核内置只读文件：

- 文件名
- 文件大小

它不做真实目录遍历。

### cat

`cat <file>` 当前输出指定内置只读文件内容。

为了降低教学环境里的输入门槛，当前还兼容：

- `/readme.txt`
- `readme.txt`
- `readmetxt`

这类等价输入形式。

## 4. 与真实文件系统的区别

当前实现还不是完整文件系统：

1. 不支持真实磁盘
2. 不支持目录树
3. 不支持写入
4. 不支持权限
5. 不支持 inode
6. 不支持 block cache
7. 不支持持久化

## 5. open / read / close 雏形

在 Task58 中，当前已经补上了教学版只读 fd 层：

```text
path
    -> readonly file object
        -> fd table
            -> open/read/close
```

当前 fd 表项最小记录：

- `used`
- `file`
- `offset`

当前语义：

- `open(path)`：根据路径查找内置只读文件，成功返回 fd
- `read(fd, buf, size)`：从当前 offset 开始读取，读完后 offset 前进
- `close(fd)`：释放 fd 表项，关闭后 fd 失效

当前 fd 从 `3` 开始分配，`0/1/2` 仅保留给后续更完整的标准输入输出语义。

## 6. 用户态 cat 与文件 syscall

在 Task59 中，文件访问链路继续往用户态推进：

```text
run cat /readme.txt
    -> exec 用户态 cat
        -> SYS_OPEN
            -> SYS_READ
                -> SYS_WRITE
                    -> SYS_CLOSE
```

这里要区分两种 `cat`：

- `cat /readme.txt`
  - shell 内建命令
- `run cat /readme.txt`
  - 用户态 `cat` 程序

用户态 `cat` 不能直接访问内核文件表，只能通过 syscall 拿到 fd 并循环读取。

## 7. 当前限制

1. 当前只支持只读普通文件
2. 暂不支持写入
3. 暂不支持 create/delete
4. 暂不支持 pipe fd
5. 暂不支持 dup/dup2
6. 暂不完整支持 stdin/stdout/stderr
7. 暂不支持真实磁盘与目录树
8. 用户指针检查仍是教学版最小实现

## 8. 用户态 ls 与文件列表 syscall

在 Task60 中，MiniOS 继续把文件系统语义从 shell 内建命令推进到用户态程序：

```text
run ls
    -> exec 用户态 ls
        -> SYS_FILE_COUNT
        -> SYS_FILE_INFO
        -> SYS_WRITE
```

这里也要区分两种 `ls`：

- `ls`
  - shell 内建命令
- `run ls`
  - 用户态 `ls` 程序

当前新增的文件列表 syscall 只服务于内置只读文件表，不是完整目录接口：

1. `SYS_FILE_COUNT()`：返回当前内置文件数量
2. `SYS_FILE_INFO(index, buf, max_len)`：复制指定文件路径到用户缓冲区，并返回文件大小

这一步的意义是让“列出文件元信息”也开始走用户态 syscall 路径，而不只是停留在 shell 内建实现中。

## 9. 用户态 stat 与文件元信息 syscall

在 Task61 中，MiniOS 继续把“查询单个文件属性”暴露给用户态程序：

```text
run stat /readme.txt
    -> exec 用户态 stat
        -> SYS_STAT(path, stat_buf)
            -> fs_builtin_file_stat(path, out)
                -> 返回 size/type
```

当前教学版 `stat` 结构只包含两项：

- `size`
- `type`

当前文件类型统一定义为：

- `readonly-file`

也就是说，当前 `stat` 不是 POSIX `stat`，只是教学版“单个文件元信息查询接口”。

## 10. Task62：RAMFS 可写内存文件系统雏形

在 Task62 中，MiniOS 继续把文件系统从“只读文件表”推进到“运行时可写内存文件”。

当前新增的是教学版 RAMFS：

1. 文件驻留在内存中
2. 系统重启后全部丢失
3. 当前只支持小文本文件
4. 当前不支持真实磁盘、inode、block cache 或持久化

当前 RAMFS 文件槽位最小记录：

- `used`
- `path`
- `content`
- `size`

当前实现的最小 shell 命令：

- `touch <file>`：创建空 RAMFS 文件
- `writefile <file> <text>`：覆盖写入文本内容
- `rm <file>`：删除 RAMFS 文件

当前文件列表 syscall 现在服务于“当前可见文件列表”，也就是：

1. 内置只读文件
2. RAMFS 内存文件

因此 `ls` / `run ls`、`cat` / `run cat`、`run stat` 都能看到 RAMFS 文件。

当前 `stat` 的类型也扩展为：

- `readonly-file`
- `ramfs-file`

## 11. 当前限制

1. 暂不支持真实磁盘
2. 暂不支持持久化
3. 暂不支持 inode
4. 暂不支持权限系统
5. 暂不支持目录树
6. 暂不支持 block cache
7. 当前 shell / 用户态都已支持最小 append，但还不是完整 POSIX `O_APPEND`
8. 暂不支持 `write(fd)`
9. 暂不支持复杂路径解析
10. 暂不支持多进程并发写保护

## 12. Task63：RAMFS fd 写入 / write syscall 雏形

在 Task63 中，MiniOS 把“RAMFS 文件写入”从 shell 内建命令继续推进到了 fd 层。

当前新增的最小链路是：

```text
run writefile /note.txt hello
    -> sys_open_write(path)
    -> 得到可写 fd
    -> sys_fd_write(fd, buf, size)
    -> sys_close(fd)
```

当前 fd 写入的最小语义：

1. 只有 RAMFS 文件可以用可写 fd 打开
2. 内置只读文件不能通过 fd 写入
3. 写入采用覆盖写语义，追加写由单独的 append 接口处理
4. 写入成功后会更新文件 size

## 13. Task64：RAMFS append 追加写入

在 Task64 中，MiniOS 继续在 RAMFS 上补齐“追加写入”语义。

当前新增的最小链路分成两条：

```text
append /note.txt world
    -> shell 内建 append
        -> fs_append_ramfs_file(path, text)
```

```text
run append /note.txt world
    -> exec 用户态 append
        -> SYS_APPEND_FILE(path, text)
            -> fs_append_ramfs_file(path, text)
```

当前 append 语义：

1. 只允许作用于 RAMFS 文件
2. 不自动创建文件，推荐先 `touch`
3. 不自动添加空格或换行
4. 从当前文件 `size` 位置继续写入
5. 成功后更新 `size`
6. 追加超过 `MAX_RAMFS_FILE_SIZE` 时失败，且不会破坏原内容

这意味着：

```text
writefile /note.txt hello
append /note.txt world
```

最终内容是：

```text
helloworld
```

这里要明确区分两种写法：

1. `writefile`
   - 覆盖写入
2. `append`
   - 追加写入

当前只读内置文件（如 `/readme.txt`）仍然禁止 append。

## 14. Task65：Shell 输出重定向到 RAMFS

在 Task65 中，MiniOS 把已有的 RAMFS 覆盖写和追加写语义，接到了 shell 的最小重定向语法上。

当前仅支持：

```text
echo text > /file
echo text >> /file
```

### `>` 当前语义

`>` 对应 RAMFS 覆盖写。

1. shell 先提取 `echo` 后、重定向符号前的文本内容
2. 如果目标是已有 RAMFS 文件，则直接覆盖旧内容
3. 如果目标文件不存在，则先创建 RAMFS 文件，再写入内容
4. 如果目标是内置只读文件，则失败

### `>>` 当前语义

`>>` 对应 RAMFS 追加写。

1. shell 提取 `echo` 输出文本
2. 目标文件必须已经存在
3. 目标文件必须是 RAMFS 文件
4. 追加时不会覆盖旧内容
5. 追加后 size 会增加
6. 超过 `MAX_RAMFS_FILE_SIZE` 时失败，且不破坏原内容

### 与 `writefile` / `append` 的关系

当前关系可以理解为：

```text
echo text > /file
    ~ writefile /file text
```

```text
echo text >> /file
    ~ append /file text
```

所以 Task65 不是重新实现新的文件系统接口，而是把已有 RAMFS 写接口接到了 shell 语法层。

### 当前限制

1. 当前只支持 `echo` 的输出重定向
2. 暂不支持通用用户程序 stdout 重定向
3. 暂不支持 `<`
4. 暂不支持 `2>` / `2>&1`
5. 暂不支持 `dup/dup2`
6. 暂不支持管道和重定向组合
7. 暂不支持后台任务重定向
8. 暂不支持复杂引号解析

## 15. Task66：用户态程序 stdout 重定向到 RAMFS

Task66 把 Task65 的 shell 语法重定向继续推进到 `run` 启动的用户态程序。

当前支持：

```text
run cat /readme.txt > /copy.txt
run ls > /files.txt
run stat /readme.txt > /stat.txt
run stat /programs >> /stat.txt
```

### process 级 stdout 重定向字段

当前 PCB 中新增了最小字段：

1. `stdout_redirect_enabled`
   - 为 1 时表示当前进程的 `SYS_WRITE` 应写入 RAMFS
2. `stdout_redirect_append`
   - 为 0 表示 shell 使用 `>`
   - 为 1 表示 shell 使用 `>>`
3. `stdout_redirect_started`
   - 只在 `>` 模式下用来区分第一次写入
4. `stdout_redirect_path`
   - 保存目标 RAMFS 文件路径

### `>` 当前语义

对 `run ... > file`：

1. 子进程第一次 `SYS_WRITE`
   - 如果目标文件不存在，则先创建 RAMFS 文件
   - 然后做一次覆盖写
2. 子进程后续 `SYS_WRITE`
   - 自动改为追加写入

这样可以保证 `run ls > /files.txt` 这类多次输出不会只留下最后一段。

### `>>` 当前语义

对 `run ... >> file`：

1. 目标文件必须已存在
2. 目标文件必须是 RAMFS 文件
3. 所有 `SYS_WRITE` 都按追加写入处理

### 与 Task65 echo 重定向的区别

```text
echo text > /file
```

- shell 直接调用 RAMFS 覆盖写/追加写接口

```text
run cat /readme.txt > /copy.txt
```

- shell 只负责配置子进程 stdout 重定向
- 真正写入发生在用户程序调用 `SYS_WRITE` 时

### 当前限制

1. 当前不是完整 `dup2` / fd stdout 重定向
2. 暂不支持 `<`
3. 暂不支持 `2>` / `2>&1`
4. 暂不支持管道和重定向组合
5. 暂不支持后台任务重定向
6. 暂不支持多个重定向
7. 暂不支持复杂 quoting
8. 当前仍不支持真实磁盘和持久化
5. 如果新内容比旧内容短，旧尾巴会被清理，避免残留脏数据

当前 shell 和用户态的两条写路径同时存在：

1. shell 内建 `writefile <file> <text>`
2. 用户态 `run writefile <file> <text>`

其中用户态程序必须通过 syscall 访问 RAMFS，不能直接修改内核文件表。

## 16. Task67：用户态程序 stdin 重定向到文件

Task67 把前一轮的 stdout 重定向继续推进成教学版 stdin 重定向。

当前支持：

```text
run cat < /readme.txt
run cat < /programs
run cat < /input.txt
```

### process 级 stdin 重定向字段

当前 PCB 中新增了最小字段：

1. `stdin_redirect_enabled`
   - 为 1 时表示当前进程的 `SYS_READ(fd=0)` 应从文件读取
2. `stdin_redirect_path`
   - 保存当前 stdin 重定向源文件路径
3. `stdin_redirect_offset`
   - 记录已经读到文件的哪个位置

### `fd=0` 当前语义

当前 `SYS_READ` 的教学版行为是：

1. 若 `fd >= 3`
   - 继续走已有 fd 表读取逻辑
2. 若 `fd == 0` 且进程启用了 stdin 重定向
   - 从 `stdin_redirect_path` 指向的文件读取
   - 读取成功后推进 `stdin_redirect_offset`
   - 到 EOF 返回 `0`
3. 若 `fd == 0` 且没有启用 stdin 重定向
   - 当前直接返回 `0`
   - 不做真实 tty/键盘交互输入

输入源允许是：

1. 内置只读文件
2. RAMFS 文件

### cat 的 stdin 模式

当前用户态 `cat` 有两种模式：

```text
run cat /readme.txt
```

- argv 文件模式：按原有 `open/read/close` 路径工作

```text
run cat < /readme.txt
```

- stdin 模式：没有文件参数时，循环 `SYS_READ(0, ...)` 直到 EOF

### 与 Task66 的关系

Task66：

```text
run cat /readme.txt > /copy.txt
```

- 用户程序输出 -> RAMFS 文件

Task67：

```text
run cat < /readme.txt
```

- 文件 -> 用户程序输入

### 当前限制

1. 当前不是完整 `dup2` / fd 复制模型
2. 暂不支持真实 tty
3. 暂不支持键盘交互 stdin
4. 暂不支持 here-doc
5. 暂不支持管道
6. 暂不支持后台输入重定向
7. 暂不支持多个输入重定向
8. 暂不支持复杂 quoting
9. 暂不支持或暂不推荐 `<` 与 `>` 组合

## 17. Task68：组合重定向 < + > / < + >>

Task68 把前两轮的 stdin/stdout 重定向组合起来，形成教学版：

```text
run cat < /readme.txt > /copy.txt
run cat < /input.txt >> /log.txt
```

### 当前数据流

组合重定向下的数据流是：

```text
输入文件 -> sys_read(0, buf, size) -> 用户程序 -> sys_write(text) -> 输出文件
```

其中：

1. `SYS_READ(fd=0)` 负责从 `stdin_redirect_path` 读取
2. `SYS_WRITE` 负责根据 `stdout_redirect_path` 把文本写入 RAMFS

### 输入文件与输出文件规则

输入文件：

1. 必须已存在
2. 可以是内置只读文件
3. 可以是 RAMFS 文件

输出文件：

1. `>`：
   - 若目标不存在，则在第一次写入时自动创建 RAMFS 文件
   - 若目标已存在且是 RAMFS 文件，则覆盖写入
2. `>>`：
   - 目标文件必须已存在
   - 目标文件必须是 RAMFS 文件
   - 写入按追加语义进行
3. 内置只读文件不能作为输出目标

### 与真实 Linux 的区别

真实系统常见做法是：

1. `open(input)`
2. `dup2(input_fd, 0)`
3. `open(output)`
4. `dup2(output_fd, 1)`
5. `exec(program)`

MiniOS 当前没有完整 `dup2` / fd 复制，因此仍采用教学版 PCB 字段：

1. `stdin_redirect_*`
2. `stdout_redirect_*`

### 当前限制

1. 暂不支持 pipe
2. 暂不支持 dup2
3. 暂不支持 fd 复制
4. 暂不支持 stderr 重定向
5. 暂不支持后台任务重定向
6. 暂不支持多个输入或多个输出重定向
7. 暂不支持复杂 quoting
8. 暂不支持真实磁盘和持久化

## 18. Task69：教学版单管道 buffer

Task69 当前新增的不是文件系统对象，而是一个教学版内核单管道缓冲区。

支持示例：

```text
run cat /readme.txt | run cat
run cat /programs | run cat
run cat /input.txt | run cat
```

### 当前执行模型

当前不是 UNIX 并发 pipe，而是顺序执行：

1. 左侧程序先运行
2. 左侧 `SYS_WRITE` 输出全部写入 pipe buffer
3. 左侧结束
4. 右侧程序再运行
5. 右侧 `SYS_READ(fd=0)` 从 pipe buffer 读取

### pipe buffer 与 RAMFS 的区别

RAMFS：

1. 是文件系统对象
2. 有路径
3. 会出现在 `ls`
4. 可以被 `cat /path` / `stat /path` 观察

pipe buffer：

1. 不是文件系统对象
2. 没有路径
3. 不会出现在 `ls`
4. 不能被 `cat /path` 访问
5. 只在单次 `run A | run B` 执行期间使用

### 当前限制

1. 暂不支持多级管道
2. 暂不支持并发 pipe
3. 暂不支持阻塞 pipe
4. 暂不支持 pipe fd
5. 暂不支持 dup2
6. 暂不支持后台管道
7. 暂不支持管道和重定向组合
8. pipe buffer 有固定容量上限

## 19. Task70：pipe buffer + RAMFS 输出文件

Task70 当前把 Task69 的 pipe buffer 再接到 Task66 的 stdout 文件重定向，形成：

```text
左程序 stdout -> pipe buffer -> 右程序 stdin -> 右程序 stdout -> RAMFS 文件
```

支持示例：

```text
run cat /readme.txt | run cat > /copy.txt
run cat /programs | run cat > /programs_copy.txt
run cat /programs | run cat >> /log.txt
```

### 当前语义

1. 左侧进程继续把 `SYS_WRITE` 输出写入 pipe buffer
2. 右侧进程继续通过 `SYS_READ(fd=0)` 从 pipe buffer 读取
3. 右侧进程的 `SYS_WRITE` 不再输出到屏幕，而是按 Task66 规则写入 RAMFS 文件
4. `>` 仍表示首次覆盖写，后续同一进程多次 `SYS_WRITE` 自动追加
5. `>>` 仍表示始终追加到已有 RAMFS 文件

### pipe buffer 与 RAMFS 文件的关系

1. pipe buffer 仍然只是临时内核缓冲区
2. pipe buffer 本身不会出现在 `ls`
3. 真正可被 `cat /path` / `stat /path` 观察的是右侧输出文件
4. 也就是说，可见结果属于 RAMFS，pipe buffer 只是中间传输层

### 当前限制

1. 暂不支持多级管道
2. 暂不支持并发 pipe
3. 暂不支持阻塞 pipe
4. 暂不支持 pipe fd
5. 暂不支持 dup2
6. 暂不支持 stdin 重定向 + pipe
7. 暂不支持后台管道
8. 暂不支持 stderr 重定向
9. 暂不支持复杂 quoting
