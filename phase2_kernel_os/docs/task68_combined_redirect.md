# Task68：组合重定向 < + > 雏形 / run ... < input > output

## 1. 本轮目标

本轮目标是让用户态程序可以同时使用 stdin 文件输入和 stdout 文件输出，例如：

```text
run cat < /readme.txt > /copy.txt
```

## 2. 为什么需要本任务

Task66 已经完成：

- 用户程序输出 -> 文件

Task67 已经完成：

- 文件 -> 用户程序输入

Task68 把两者组合起来，形成最小教学版单进程文件数据流：

```text
输入文件 -> 用户程序 -> 输出文件
```

## 3. 当前支持语法

当前支持：

```text
run cat < /readme.txt > /copy.txt
run cat < /input.txt > /output.txt
run cat < /input.txt >> /log.txt
```

## 4. 数据流

组合重定向下的数据流是：

```text
输入文件 -> sys_read(0) -> 用户程序 -> sys_write -> 输出文件
```

其中：

1. `SYS_READ(fd=0)` 从 stdin 重定向文件读取
2. `SYS_WRITE` 按 stdout 重定向配置写入输出文件

## 5. shell 解析规则

当前 shell 会：

1. 识别 `<`
2. 识别 `>` 或 `>>`
3. 把 `<`、`>`、`>>` 以及对应路径从用户程序 argv 中剥离
4. 只把真正的程序参数传给用户程序

例如：

```text
run cat < /readme.txt > /copy.txt
```

最终传给 `cat` 的 argv 只表示：

```text
cat
```

## 6. process 重定向字段

当前 PCB 中两套教学版重定向字段可以同时启用：

1. stdin 重定向：
   - `stdin_redirect_enabled`
   - `stdin_redirect_path`
   - `stdin_redirect_offset`
2. stdout 重定向：
   - `stdout_redirect_enabled`
   - `stdout_redirect_path`
   - `stdout_redirect_append`
   - `stdout_redirect_started`

## 7. SYS_READ / SYS_WRITE 行为

组合重定向下：

1. `SYS_READ(fd=0)`
   - 从输入文件读取
   - 读到 EOF 返回 `0`
2. `SYS_WRITE`
   - 根据 stdout 配置写屏幕或写 RAMFS
   - `>`：第一次覆盖，后续追加
   - `>>`：始终追加

## 8. 与真实 Linux dup2 的区别

真实系统通常依赖：

1. `open`
2. `dup2`
3. `exec`

MiniOS 当前没有完整 `dup2` / fd 复制，因此仍采用教学版 process 字段方案，而不是完整 fd 级重定向。

## 9. 当前限制

1. 暂不支持 pipe
2. 暂不支持 dup2
3. 暂不支持 fd 复制
4. 暂不支持 stderr 重定向
5. 暂不支持 `2>&1`
6. 暂不支持 here-doc
7. 暂不支持后台任务重定向
8. 暂不支持多个输入/输出重定向
9. 暂不支持复杂 quoting
10. 暂不支持真实磁盘和持久化
11. 当前仍不支持 `run cat /readme.txt < /input.txt` 这类 argv 文件模式和 stdin 重定向混用

## 10. 验证方式

```text
run cat < /readme.txt > /copy.txt
cat /copy.txt

touch /input.txt
writefile /input.txt hello
run cat < /input.txt > /output.txt
cat /output.txt

run cat < /not_exist.txt > /bad.txt
run cat < /readme.txt > /readme.txt
```
