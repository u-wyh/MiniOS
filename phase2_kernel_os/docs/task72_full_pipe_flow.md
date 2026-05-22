# Task72：完整单管道数据流雏形 / run A < input | run B > output

## 1. 本轮目标

本轮目标是支持教学版：

```text
run cat < /readme.txt | run cat > /copy.txt
```

也就是把文件输入、单管道和文件输出组合成一条完整数据流。

## 2. 为什么需要本任务

Task70 已经支持：

- `A -> pipe -> B -> file`

Task71 已经支持：

- `file -> A -> pipe -> B`

本轮把两者合并，形成：

- `file -> A -> pipe -> B -> file`

## 3. 当前支持语法

当前支持：

```text
run cat < /readme.txt | run cat > /copy.txt
run cat < /programs | run cat > /programs_copy.txt
run cat < /input.txt | run cat > /output.txt
run cat < /input.txt | run cat >> /log.txt
```

## 4. 数据流

当前数据流是：

```text
输入文件 -> 左程序 stdin -> 左程序 stdout -> pipe buffer -> 右程序 stdin -> 右程序 stdout -> 输出文件
```

## 5. shell 解析规则

当前 shell 规则：

1. `<` 和输入路径会从左侧 argv 中剥离
2. `>` / `>>` 和输出路径会从右侧 argv 中剥离
3. `|` 不会进入任一侧 argv
4. 左右两侧都必须是 `run ...`
5. 多个 `<`、多个 `>`、多个 `|`、非 `run` 命令参与管道都会报错

## 6. process 重定向字段组合

左侧进程当前会同时启用：

1. `stdin_redirect_enabled`
2. `stdin_redirect_path`
3. `stdin_redirect_offset`
4. `stdout_redirect_to_pipe`

右侧进程当前会同时启用：

1. `stdin_redirect_from_pipe`
2. `stdout_redirect_enabled`
3. `stdout_redirect_path`
4. `stdout_redirect_append`
5. `stdout_redirect_started`

## 7. SYS_READ / SYS_WRITE 行为

左侧进程：

1. `SYS_READ(fd=0)` 从输入文件读取
2. `SYS_WRITE` 把输出写入 pipe buffer

右侧进程：

1. `SYS_READ(fd=0)` 从 pipe buffer 读取
2. `SYS_WRITE` 把输出写入 RAMFS 文件

## 8. 当前限制

当前仍不支持：

1. 多级管道
2. 并发 pipe
3. 阻塞 pipe
4. pipe fd
5. dup2
6. 后台管道
7. stderr 重定向
8. 2>&1
9. 复杂 quoting

后续可以继续扩展：

1. 真正的 pipe fd
2. 并发调度版 pipe
3. 更接近 UNIX 的 `dup2` / `fd 0/1/2`

## 9. 验证方式

本轮重点验证：

```text
run cat < /readme.txt | run cat > /copy.txt
cat /copy.txt
touch /input.txt
writefile /input.txt hello
run cat < /input.txt | run cat > /output.txt
cat /output.txt
run cat < /not_exist.txt | run cat > /bad.txt
```

同时确认未破坏：

- `README.md`
- `docs/phase2.md`
- `docs/fs.md`
- `docs/syscall.md`
- `docs/user_programs.md`
