# Task73：用户态 wc 程序 / stdin 数据流验证

## 1. 本轮目标

本轮目标是新增一个最小用户态 `wc` 程序，用来验证：

- 文件 stdin 重定向
- pipe stdin
- stdout 输出
- stdout 重定向到 RAMFS 文件

这说明 MiniOS 当前的数据流不仅能“复制文本”，还已经能让用户态程序真正处理输入数据。

## 2. 为什么需要本任务

前面的 `cat` 更像“把输入原样搬运到输出”。  
而 `wc` 会对输入进行统计，能证明：

```text
stdin -> 用户程序处理 -> stdout
```

这条链路已经具备教学版可用性。

## 3. wc 当前语义

当前 `wc` 为教学版最小实现，统一从 `stdin` 读取，不读取 argv 文件路径。

当前输出：

```text
bytes: <n>
lines: <n>
words: <n>
```

其中：

1. `bytes`：输入总字节数
2. `lines`：`\n` 数量
3. `words`：按空白分隔的单词数

## 4. 依赖 syscall

`wc` 当前主要依赖：

1. `sys_read(0, buf, size)`
2. `sys_write(text)`
3. `sys_exit(status)`

也就是说：

1. 输入统一来自 `fd=0`
2. 输出统一走 `stdout`

## 5. 验证方式

本轮主要验证：

```text
run wc
run wc < /readme.txt
run cat /readme.txt | run wc
touch /input.txt
writefile /input.txt hello
run wc < /input.txt
append /input.txt world
run wc < /input.txt
run cat < /input.txt | run wc > /count.txt
cat /count.txt
```

## 6. 当前限制

1. 暂不支持 Linux `wc` 参数
2. 暂不支持 `-l` / `-w` / `-c`
3. 暂无交互式 tty stdin
4. 暂不支持 `run wc /file`
5. 当前统一通过 stdin 读取
6. 后续可以继续扩展更多文本处理型用户程序
