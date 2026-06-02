# Task88：pipe read/write 端关闭语义整理

## 1. 任务目标

本轮目标不是实现完整 POSIX pipe，而是把当前教学版 pipe 的 close 路径整理清楚：

1. `close(pipe read fd)` 的行为明确
2. `close(pipe write fd)` 的行为明确
3. 写端关闭后的 EOF 语义明确
4. 读端关闭后的 write 行为明确
5. 进程 `exit` 前的 fd 清理与 pipe 状态关系明确

## 2. 为什么需要 pipe close 语义

前几轮已经具备：

1. `pipe()`
2. `dup2()`
3. `fork()` 后 fd 继承
4. 用户态 `pipe + fork + dup2` 组合测试

但如果没有 close 语义，后续更接近 UNIX 的数据流实验会很混乱：

1. 无法定义“写端全部结束后，读端什么时候 EOF”
2. 无法定义“读端关闭后，写端继续写会怎样”
3. 无法明确进程退出时 pipe fd 怎样清理

## 3. 修改文件

1. `include/process.h`
2. `kernel/process.c`
3. `kernel/process_parts/core_helpers.inc`
4. `kernel/process_parts/fd_and_input.inc`
5. `kernel/process_parts/runtime_wait_sleep.inc`
6. `kernel/process_parts/redirect_pipe.inc`
7. `kernel/user_program_sources/pipe_close_test_elf_source.c`
8. `kernel/user_program_blobs/pipe_close_test_elf.inc`
9. `kernel/fs_parts/embedded_and_tables.inc`
10. `include/user_program.h`
11. `include/fs.h`
12. `readme.md`
13. `docs/phase2.md`
14. `docs/fd.md`
15. `docs/process.md`
16. `docs/pipe.md`
17. `docs/user_programs.md`

## 4. 实现思路

1. 给当前教学版全局 pipe buffer 增加 `read_open / write_open`
2. `close(pipe read fd)` 时，把读端标记为关闭
3. `close(pipe write fd)` 时，把写端标记为关闭
4. 不在 close 时直接清掉已有 buffer 数据
5. `process_exit()` 前，先关闭当前进程 still-open 的教学版 fd
6. 新增 `pipe_close_test` 做最小用户态验证

## 5. 核心语义

当前教学版规则是：

1. 关闭读端：
   - 标记 `read_open = 0`
   - 后续写端写入返回错误或 0
2. 关闭写端：
   - 标记 `write_open = 0`
   - 读端仍能把已有数据读完
   - 读完后继续 `read` 返回 EOF
3. 重复 close：
   - 返回错误
   - 不 panic

## 6. 验证命令

```text
make clean
make
make run
run pipe_close_test
run pipe_test
run dup2_test
run fork_fd_test
run pipe_fork_dup2_test
run cat /readme.txt | run wc
run cat < /readme.txt | run grep MiniOS > /grep.txt
cat /grep.txt
```

## 7. 当前限制

1. 当前仍然只有一个全局教学版 pipe buffer
2. 当前没有 fd 引用计数
3. 当前不是完整 POSIX 多引用 close 语义
4. 当前不支持 SIGPIPE
5. 当前不支持 EPIPE errno
6. 当前不支持阻塞 read/write
7. 当前不支持真正并发 pipe

## 8. 后续方向

1. Task89：exec 与 fd 保留语义整理
2. Task90：用户态 pipeline demo
3. Task91：并发 pipe / 阻塞读写雏形
