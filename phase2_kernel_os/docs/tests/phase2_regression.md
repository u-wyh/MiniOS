# MiniOS Phase2 回归测试清单

## 1. 测试前准备

建议每次在较大修改后都按下面顺序进行：

```text
make clean
make
make run
```

进入 shell 后，再按下面各模块逐项执行。

若某项失败，建议记录：

1. 失败命令
2. 实际输出
3. 是否 panic / 卡死 / 返回 shell

## 2. RAMFS 基础测试

命令：

```text
touch /reg.txt
writefile /reg.txt hello
append /reg.txt world
cat /reg.txt
ls /
```

预期：

1. 文件可以创建
2. 文件可以写入
3. 文件可以追加
4. `cat /reg.txt` 可以读出内容
5. `ls /` 能看到新文件

## 3. 基础用户程序测试

命令：

```text
run cat /readme.txt
run wc < /readme.txt
run grep MiniOS < /readme.txt
run head -n 3 < /readme.txt
run tail -n 3 < /readme.txt
run sort < /readme.txt
```

预期：

1. `cat` 正常输出
2. `wc` 正常统计
3. `grep` 能过滤 `MiniOS`
4. `head` 输出前 3 行
5. `tail` 输出后 3 行
6. `sort` 输出排序结果

## 4. redirect 测试

命令：

```text
run grep MiniOS < /readme.txt > /grep.txt
cat /grep.txt

run grep MiniOS < /readme.txt > /append.txt
run grep MiniOS < /readme.txt >> /append.txt
cat /append.txt
```

预期：

1. `<` 正常作为 stdin
2. `>` 正常覆盖写
3. `>>` 正常追加写
4. redirect 符号不进入用户程序 `argv`

## 5. pipe 基础测试

命令：

```text
run cat /readme.txt | run wc
run cat /readme.txt | run grep MiniOS
run cat /readme.txt | run head -n 3
```

预期：

1. 左侧 stdout 进入 pipe
2. 右侧 stdin 从 pipe 读取
3. 右侧 argv 保留正确

## 6. Shell 多级管道测试

命令：

```text
run cat /readme.txt | run grep MiniOS | run wc
run cat /readme.txt | run head -n 5 | run tail -n 2
```

预期：

1. `N` 个命令创建 `N-1` 个 pipe
2. 中间命令同时连接 stdin/stdout
3. 不 panic
4. 不死锁

## 7. pipe + redirect 组合测试

命令：

```text
run cat /readme.txt | run grep MiniOS | run wc > /wc.txt
cat /wc.txt

run cat < /readme.txt | run grep MiniOS | run wc
```

预期：

1. 最后一段输出重定向正常
2. 第一段输入重定向正常
3. pipe 和 redirect 不互相破坏

## 8. pipe object 多实例测试

命令：

```text
run pipe_multi_test
run pipe_test
run pipe_test
run pipe_test
```

预期：

1. `pipe_multi_test` 通过
2. 多个 pipe object 数据不串
3. 连续 `pipe_test` 没有残留状态

## 9. close / dup2 / fork / exec 测试

命令：

```text
run pipe_close_test
run dup2_test
run fork_fd_test
run pipe_fork_dup2_test
run exec_fd_test
run exec_args_test
```

预期：

1. pipe close 语义正常
2. `dup2` 能复制 fd 绑定
3. fork 后 fd 继承正常
4. `pipe + fork + dup2` 正常
5. exec 后 fd table 保留
6. exec 的 argv 传递正常

## 10. mini_pipeline 测试

命令：

```text
run mini_pipeline cat /readme.txt -- grep MiniOS
run mini_pipeline cat /readme.txt -- head -n 3
run mini_pipeline cat /readme.txt -- grep MiniOS -- wc
run mini_pipeline cat /readme.txt -- head -n 5 -- tail -n 2
```

预期：

1. 二段 `mini_pipeline` 正常
2. 多级 `mini_pipeline` 正常
3. 每段 argv 保留正确
4. 多个 pipe object 不串数据

## 11. 错误输入测试

命令：

```text
run
run grep <
run cat /readme.txt |
| run wc
run cat /readme.txt | | run wc
run mini_pipeline
run mini_pipeline --
run mini_pipeline cat /readme.txt --
```

预期：

1. 输出错误提示
2. 不 panic
3. shell 后续仍可继续执行命令

## 12. 连续运行稳定性测试

命令：

```text
run mini_pipeline cat /readme.txt -- grep MiniOS -- wc
run cat /readme.txt | run grep MiniOS | run wc
run pipe_multi_test
run pipe_close_test
run grep MiniOS < /readme.txt > /final.txt
cat /final.txt
```

预期：

1. 多次 pipe / redirect / pipeline 后系统仍稳定
2. 没有残留 pipe 数据
3. 没有 fd 状态污染

## 13. 当前已知限制

请如实按当前实现理解这些测试：

1. 不是完整 POSIX
2. pipe 引用计数仍然教学化
3. signal / SIGPIPE 未实现
4. job control 未实现
5. tty 未完整实现
6. shell 不支持引号、转义、环境变量、通配符
7. pipeline 不支持后台任务和进程组
8. 文件系统仍是 RAMFS，不是磁盘 FS
