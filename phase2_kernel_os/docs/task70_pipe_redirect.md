# Task70：管道 + 输出重定向组合雏形 / run A | run B > file

## 1. 本轮目标

本轮目标是支持：

```text
run cat /readme.txt | run cat > /copy.txt
```

也就是把单管道结果继续写入 RAMFS 文件。

## 2. 为什么需要本任务

Task69 已经支持：

```text
用户程序 A stdout -> pipe buffer -> 用户程序 B stdin
```

Task70 继续推进成：

```text
用户程序 A stdout -> pipe buffer -> 用户程序 B stdin -> 用户程序 B stdout -> RAMFS 文件
```

这让 MiniOS 在教学版顺序执行模型下，具备了更接近真实 shell 的“进程间数据流 + 文件输出”能力。

## 3. 当前支持语法

当前支持：

```text
run cat /readme.txt | run cat > /copy.txt
run cat /programs | run cat > /programs_copy.txt
run cat /programs | run cat >> /log.txt
```

## 4. 数据流

当前数据流是：

```text
左程序 stdout -> pipe buffer -> 右程序 stdin -> 右程序 stdout -> RAMFS 文件
```

执行顺序仍然是教学版顺序执行：

1. 左侧先完整运行
2. 左侧 `SYS_WRITE` 写 pipe buffer
3. 左侧结束
4. 右侧再运行
5. 右侧 `SYS_READ(fd=0)` 从 pipe buffer 读取
6. 右侧 `SYS_WRITE` 再写目标 RAMFS 文件

## 5. shell 解析规则

当前 shell 的最小规则是：

1. 只支持单个 `|`
2. 左右两侧都必须是 `run ...`
3. `|` 不会进入任一侧的用户程序 `argv`
4. 右侧的 `>` / `>>` 以及目标路径，也不会进入右侧用户程序 `argv`
5. 左侧带重定向、右侧带 `<`、多级管道等复杂形式当前都会拒绝

## 6. process 重定向字段组合

左侧进程当前启用：

1. `stdout_redirect_to_pipe`

右侧进程当前启用：

1. `stdin_redirect_from_pipe`
2. `stdout_redirect_enabled`
3. `stdout_redirect_append`
4. `stdout_redirect_started`
5. `stdout_redirect_path`

也就是说，右侧进程当前可以同时具备：

```text
stdin <- pipe
stdout -> file
```

## 7. SYS_READ / SYS_WRITE 行为

左侧进程：

1. `SYS_WRITE`
   - 命中 `stdout -> pipe`
   - 文本写入 pipe buffer
   - 不输出到屏幕

右侧进程：

1. `SYS_READ(fd=0)`
   - 命中 `stdin <- pipe`
   - 从 pipe buffer 读取
2. `SYS_WRITE`
   - 因为右侧没有启用 `stdout -> pipe`
   - 所以继续走 Task66 的 `stdout -> file`

## 8. 当前限制

1. 暂不支持多级管道
2. 暂不支持并发 pipe
3. 暂不支持阻塞 pipe
4. 暂不支持 pipe fd
5. 暂不支持 dup2
6. 暂不支持 stdin 重定向 + pipe
7. 暂不支持后台管道
8. 暂不支持 stderr 重定向
9. 暂不支持复杂 quoting
10. 当前左侧程序失败时，右侧可能仍会把 pipe 中已有文本继续处理
11. 后续可扩展真正 pipe fd、并发调度和更完整的 `dup2`

## 9. 验证方式

```text
run cat /readme.txt | run cat > /copy.txt
cat /copy.txt
run cat /programs | run cat > /programs_copy.txt
cat /programs_copy.txt
run cat /programs | run cat >> /log.txt
cat /log.txt
run cat /readme.txt | run cat > /readme.txt
run cat /readme.txt | run cat > 
run cat /readme.txt | run cat | run cat > /x.txt
```
