# Task86：fork 后 fd 继承语义整理

## 1. 任务目标

整理 fork 时子进程如何继承父进程当前已有的教学版 fd 视图。

目标包括：

1. 继承普通文件 fd
2. 继承 pipe read fd
3. 继承 pipe write fd
4. 继承当前 `stdin/stdout` 绑定关系

## 2. 为什么现在做

Task84 和 Task85 已经有：

1. 用户态 `pipe()`
2. 用户态 `dup2()`

接下来如果要继续做更真实的：

1. `fork + pipe`
2. `fork + dup2`
3. 后续用户态自己组合 shell 风格数据流

就必须先让子进程继承父进程已有 fd。

## 3. 修改文件

1. `include/process.h`
2. `include/user_program.h`
3. `include/fs.h`
4. `kernel/process.c`
5. `kernel/process_parts/core_helpers.inc`
6. `kernel/process_parts/fork_and_reporting.inc`
7. `kernel/fs_parts/embedded_and_tables.inc`
8. `kernel/user_program_sources/fork_fd_test_elf_source.c`
9. `kernel/user_program_blobs/fork_fd_test_elf.inc`
10. `readme.md`
11. `docs/phase2.md`
12. `docs/fd.md`
13. `docs/process.md`
14. `docs/pipe.md`
15. `docs/user_programs.md`

## 4. 实现思路

1. 新增 `process_copy_fd_table(child, parent)`
2. fork 后对子进程先清空 fd table
3. 再复制父进程当前有效 fd 表项
4. 同步复制当前 `stdin/stdout` 兼容字段
5. 不引入引用计数
6. 不实现完整 file object 共享

## 5. 核心语义

当前 fork 后：

1. 子进程会继承普通文件 fd
2. 子进程会继承 pipe read/write fd
3. 子进程会继承当前 `fd=0 / fd=1` 所看到的绑定关系
4. 当前继承方式是教学版浅拷贝 / 视图复制
5. 不是完整 POSIX 共享 open file object

## 6. 验证命令

```text
run fork_fd_test
run pipe_test
run dup2_test
run cat /readme.txt | run wc
run cat < /readme.txt | run grep MiniOS > /grep.txt
run cat < /readme.txt
run cat /readme.txt > /copy.txt
cat /copy.txt
touch /dup2test.txt
writefile /dup2test.txt hello
append /dup2test.txt world
cat /dup2test.txt
```

## 7. 当前限制

1. 不是完整 POSIX fork fd 继承
2. 没有引用计数
3. 没有共享 file object
4. pipe 仍然只有一个全局教学版缓冲区
5. 不支持并发 pipe / 阻塞读写 / 多个 pipe object

## 8. 后续方向

1. Task87 可继续做用户态 `fork + pipe + dup2` 组合测试
2. Task88 可继续探索并发 pipe / 阻塞读写
3. 后续可继续推进更真实的 file object / 引用计数模型
