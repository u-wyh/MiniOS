# Task66：用户态程序 stdout 重定向到 RAMFS / run ... > file

## 1. 本轮目标

本轮目标是把用户态程序的 `sys_write` 输出重定向到 RAMFS 文件。

## 2. 为什么需要本任务

Task65 只支持 `echo` 专用重定向，本轮继续推进到 `run` 启动的用户程序，让 `cat` / `ls` / `stat` 的 stdout 也能落到文件里。

## 3. 当前支持语法

当前支持：

```text
run cat /readme.txt > /copy.txt
run ls > /files.txt
run stat /readme.txt > /stat.txt
run stat /programs >> /stat.txt
```

## 4. process stdout 重定向字段

当前 PCB 中新增：

1. `stdout_redirect_enabled`
2. `stdout_redirect_append`
3. `stdout_redirect_started`
4. `stdout_redirect_path`

它们共同描述：

1. 当前进程是否启用 stdout 重定向
2. 使用 `>` 还是 `>>`
3. `>` 是否已经完成第一次覆盖写
4. 目标 RAMFS 文件路径

## 5. SYS_WRITE 重定向行为

当前 `SYS_WRITE` 的行为分成两种：

1. 没有启用 stdout 重定向
   - 继续输出到屏幕
2. 启用了 stdout 重定向
   - 不再输出到屏幕
   - 改为写入 RAMFS 文件

## 6. > 和 >> 语义

### `>`

`run ... > file` 当前语义：

1. 第一次 `SYS_WRITE`
   - 覆盖写入目标文件
   - 若目标不存在，则先创建 RAMFS 文件
2. 后续 `SYS_WRITE`
   - 统一改为追加写入

这样可以保证 `ls` / `stat` / `cat` 多次输出时文件里保留完整结果。

### `>>`

`run ... >> file` 当前语义：

1. 目标文件必须已存在
2. 目标文件必须是 RAMFS 文件
3. 所有 `SYS_WRITE` 都按追加写处理

## 7. 与 Task65 echo 重定向的区别

```text
echo text > file
```

- shell 直接写 RAMFS

```text
run program > file
```

- 用户程序正常调用 `SYS_WRITE`
- 内核根据当前进程 stdout 重定向配置改写输出去向

## 8. 当前限制

1. 暂不支持 `dup2`
2. 暂不支持 fd 复制
3. 暂不支持 stdin 重定向
4. 暂不支持 stderr 重定向
5. 暂不支持 `2>&1`
6. 暂不支持管道和重定向组合
7. 暂不支持后台任务重定向
8. 暂不支持多个重定向
9. 暂不支持复杂 quoting
10. 暂不支持真实磁盘和持久化
11. 后续可以扩展真正 stdout fd、`dup2`、pipe

## 9. 验证方式

建议验证：

```text
run cat /readme.txt > /copy.txt
cat /copy.txt
run ls > /files.txt
cat /files.txt
run stat /readme.txt > /stat.txt
cat /stat.txt
run stat /programs >> /stat.txt
cat /stat.txt
```
