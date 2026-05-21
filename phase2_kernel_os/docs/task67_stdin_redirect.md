# Task67：用户态程序 stdin 重定向到 RAMFS / run ... < file

## 1. 本轮目标

支持最小教学版 stdin 重定向，让用户可以执行：

```text
run cat < /readme.txt
run cat < /input.txt
```

并让用户态 `cat` 在没有文件参数时，从 `fd=0` 读取文件内容。

## 2. 为什么需要本任务

Task66 已经把：

- 用户程序输出 -> 文件

这条链路打通了。

Task67 继续补齐：

- 文件 -> 用户程序输入

这样更接近真实 shell 中的 `< file` 输入重定向体验，也为后续真正的 `fd 0/1/2`、`dup2`、pipe 打基础。

## 3. 当前支持语法

当前最小支持：

```text
run cat < /readme.txt
run cat < /programs
run cat < /input.txt
```

当前只支持 `run` 命令的输入重定向，不支持普通 shell 内建命令的 `<`。

## 4. process stdin 重定向字段

当前 PCB 中新增了最小字段：

1. `stdin_redirect_enabled`
2. `stdin_redirect_path`
3. `stdin_redirect_offset`

含义是：

1. `enabled=0`
   - `SYS_READ(fd=0)` 不走文件输入
2. `enabled=1`
   - `SYS_READ(fd=0)` 从 `stdin_redirect_path` 指向的文件读取
3. `offset`
   - 记录当前已经从输入文件读取到哪里

## 5. SYS_READ fd=0 行为

当前教学版行为：

1. `fd >= 3`
   - 保持原有 fd 读取逻辑
2. `fd == 0` 且启用 stdin 重定向
   - 从输入文件读取
   - 更新 `stdin_redirect_offset`
   - EOF 返回 `0`
3. `fd == 0` 且未启用 stdin 重定向
   - 当前直接返回 `0`
   - 不实现真实 tty/键盘 stdin

## 6. cat stdin 模式

当前用户态 `cat` 分两种模式：

```text
run cat /readme.txt
```

- argv 文件模式

```text
run cat < /readme.txt
```

- stdin 模式：无文件参数时循环 `sys_read(0, ...)`

## 7. 与 Task66 stdout 重定向的关系

Task66：

- 用户程序输出 -> 文件

Task67：

- 文件 -> 用户程序输入

当前两者已经形成最小闭环，但本轮仍不支持：

```text
run cat < /readme.txt > /copy.txt
```

这种输入/输出组合重定向。

## 8. 当前限制

1. 暂不支持 dup2
2. 暂不支持 fd 复制
3. 暂不支持真实 tty
4. 暂不支持键盘交互 stdin
5. 暂不支持 here-doc
6. 暂不支持管道
7. 暂不支持后台输入重定向
8. 暂不支持多个输入重定向
9. 暂不支持复杂 quoting
10. 暂不支持或暂不推荐 `<` 与 `>` 组合
11. 后续可扩展真正 fd 0/1/2、dup2、pipe

## 9. 验证方式

```text
run cat < /readme.txt
run cat < /programs
touch /input.txt
writefile /input.txt hello
append /input.txt world
run cat < /input.txt
run cat < /not_exist.txt
```
