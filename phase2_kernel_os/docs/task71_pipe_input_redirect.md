# Task71：管道 + 输入重定向组合雏形 / run A < input | run B

## 1. 本轮目标

本轮目标是支持教学版：

```text
run cat < /readme.txt | run cat
```

也就是把文件输入接到管道左侧程序，再把左侧程序输出通过 pipe buffer 送给右侧程序。

## 2. 为什么需要本任务

Task67 已经支持：

- 文件 -> 用户程序 stdin

Task69 已经支持：

- 用户程序 stdout -> pipe buffer -> 用户程序 stdin

本轮把两者组合起来，形成：

- 文件 -> 用户程序 A -> 用户程序 B -> 屏幕

## 3. 当前支持语法

当前支持：

```text
run cat < /readme.txt | run cat
run cat < /programs | run cat
run cat < /input.txt | run cat
```

## 4. 数据流

当前数据流是：

```text
输入文件 -> 左程序 stdin -> 左程序 stdout -> pipe buffer -> 右程序 stdin -> 屏幕
```

## 5. shell 解析规则

当前 shell 规则：

1. 只支持单个 `|`
2. `<` 和输入路径会从左侧 argv 中剥离
3. `|` 不会进入左右两侧 argv
4. 左右两侧都必须是 `run ...`
5. 多个 `<`、多级管道、非 `run` 命令参与管道都会报错

## 6. process 重定向字段组合

左侧进程当前会同时启用：

1. `stdin_redirect_enabled`
2. `stdin_redirect_path`
3. `stdin_redirect_offset`
4. `stdout_redirect_to_pipe`

右侧进程当前会启用：

1. `stdin_redirect_from_pipe`

## 7. SYS_READ / SYS_WRITE 行为

左侧进程：

1. `SYS_READ(fd=0)` 从输入文件读取
2. `SYS_WRITE` 把输出写入 pipe buffer

右侧进程：

1. `SYS_READ(fd=0)` 从 pipe buffer 读取
2. `SYS_WRITE` 正常输出到屏幕

## 8. 当前限制

当前仍不支持：

1. 多级管道
2. 并发 pipe
3. 阻塞 pipe
4. pipe fd
5. dup2
6. 后台管道
7. `run A < input | run B > output`
8. 复杂组合
9. 复杂 quoting

后续可以继续扩展：

1. `run A < input | run B > output`
2. 真正的 pipe fd
3. 并发调度版 pipe

## 9. 验证方式

本轮重点验证：

```text
run cat < /readme.txt | run cat
run cat < /programs | run cat
touch /input.txt
writefile /input.txt hello
run cat < /input.txt | run cat
run cat < /not_exist.txt | run cat
```

同时确认未破坏：

- `README.md`
- `docs/phase2.md`
- `docs/fs.md`
- `docs/syscall.md`
- `docs/user_programs.md`
