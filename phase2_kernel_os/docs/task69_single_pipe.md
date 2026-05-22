# Task69：单管道 | 雏形 / 用户程序 stdout 接 stdin

## 1. 本轮目标

本轮目标是支持最小教学版单管道：

```text
run cat /readme.txt | run cat
```

## 2. 为什么需要本任务

Task66 已经把用户程序 stdout 重定向到文件。  
Task67 已经把文件输入重定向到用户程序 stdin。  
Task68 已经把文件输入和文件输出组合起来。

Task69 继续推进：不再经过文件，而是把一个进程的 stdout 接到另一个进程的 stdin。

## 3. 当前支持语法

当前支持：

```text
run cat /readme.txt | run cat
run cat /programs | run cat
run cat /input.txt | run cat
```

## 4. 当前执行模型

当前不是 UNIX 并发 pipe，而是顺序执行：

1. 左侧程序先运行
2. 左侧 stdout 写入教学版 pipe buffer
3. 左侧结束
4. 右侧程序再运行
5. 右侧 stdin 从 pipe buffer 读取

当前教学版顺序模型还意味着：

1. 左侧程序如果把错误文本写到 `stdout`
2. 这些文本也会先进入 pipe buffer
3. 右侧程序随后会继续把这部分内容读出来

## 5. pipe buffer 结构

当前使用一个全局教学版 pipe buffer，最小字段包括：

1. `used`
2. `data`
3. `size`
4. `read_offset`

pipe buffer 有固定容量上限，不做动态扩容。

## 6. SYS_WRITE 行为

当左侧进程启用了 `stdout_redirect_to_pipe` 时：

1. `SYS_WRITE` 不再输出到屏幕
2. 文本会追加写入 pipe buffer
3. 超过容量上限时失败

## 7. SYS_READ 行为

当右侧进程启用了 `stdin_redirect_from_pipe` 时：

1. `SYS_READ(fd=0)` 从 pipe buffer 读取
2. 每次读取后推进 `read_offset`
3. 读到 EOF 返回 `0`

## 8. 与真实 UNIX pipe 的区别

当前不是：

1. pipe fd
2. 并发执行
3. 阻塞读写
4. dup2
5. 多级管道

这是教学版顺序执行单管道。

## 9. 当前限制

1. 暂不支持多级管道
2. 暂不支持并发管道
3. 暂不支持阻塞 pipe
4. 暂不支持 pipe fd
5. 暂不支持 dup2
6. 暂不支持后台管道
7. 暂不支持管道和重定向组合
8. 暂不支持复杂 quoting
9. pipe buffer 有固定容量
10. 左侧程序失败时，当前默认不阻止右侧继续运行
11. 后续可扩展真正 pipe fd 与并发调度

## 10. 验证方式

```text
run cat /readme.txt | run cat
run cat /programs | run cat
touch /input.txt
writefile /input.txt hello
run cat /input.txt | run cat
run cat /not_exist.txt | run cat
run cat /readme.txt | run cat | run cat
run cat /readme.txt > /x.txt | run cat
```
