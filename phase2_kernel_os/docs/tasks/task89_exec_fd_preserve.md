# Task89：exec 与 fd 保留语义整理

## 1. 任务目标

本轮目标不是实现完整 POSIX exec，而是整理当前教学版 exec 的一个关键默认语义：

1. exec 替换当前进程用户镜像
2. 默认保留当前进程 fd table
3. exec 后 `fd=0 / fd=1` 仍然有效
4. 为后续用户态 pipeline demo 做准备

## 2. 为什么需要这一步

真实 UNIX 里，shell 做管道和重定向的关键就是：

1. 先通过 `dup2()` 改好 `fd=0 / fd=1`
2. 再 `exec()` 目标程序
3. 新程序并不知道自己被重定向了，它只管 `read(0)` / `write(1)`

如果 exec 会清空 fd table，这条路线就断掉了。

## 3. 修改文件

1. `include/user_program.h`
2. `include/fs.h`
3. `kernel/fs_parts/embedded_and_tables.inc`
4. `kernel/process_parts/exec_create.inc`
5. `kernel/user_program_sources/exec_fd_test_elf_source.c`
6. `kernel/user_program_sources/exec_fd_writer_elf_source.c`
7. `kernel/user_program_sources/exec_fd_reader_elf_source.c`
8. `kernel/user_program_blobs/exec_fd_test_elf.inc`
9. `kernel/user_program_blobs/exec_fd_writer_elf.inc`
10. `kernel/user_program_blobs/exec_fd_reader_elf.inc`
11. `readme.md`
12. `docs/phase2.md`
13. `docs/fd.md`
14. `docs/process.md`
15. `docs/pipe.md`
16. `docs/user_programs.md`

## 4. 实现思路

1. 先梳理当前 `process_exec_program_args()` 路径
2. 明确当前实现并不会清空 fd table
3. 新增 `exec_fd_writer`：只往 `fd=1` 写固定文本
4. 新增 `exec_fd_reader`：从 `fd=0` 读，再往 `fd=1` 输出
5. 新增 `exec_fd_test`：验证 `dup2(pipe_write_fd, 1)` 后再 exec，stdout 绑定仍然保留

## 5. 核心语义

当前教学版 exec 规则：

1. 替换当前进程的用户镜像与返回现场
2. 默认保留 fd table
3. 不主动关闭普通文件 fd / pipe fd
4. 不重置 `fd=0 / fd=1`
5. 当前没有 close-on-exec

## 6. 验证命令

```text
make clean
make
make run
run exec_fd_test
run pipe_test
run dup2_test
run fork_fd_test
run pipe_fork_dup2_test
run pipe_close_test
run cat /readme.txt | run wc
run cat < /readme.txt | run grep MiniOS > /grep.txt
cat /grep.txt
```

## 7. 当前限制

1. 当前不是完整 POSIX exec
2. 当前没有 close-on-exec
3. 当前没有环境变量
4. 当前没有完整 argv/envp
5. 当前仍然只有一个全局教学版 pipe buffer
6. 当前还不是完整 UNIX pipeline

## 8. 后续方向

1. Task90：用户态 pipeline demo
2. Task91：exec 参数传递整理
3. Task92：阻塞 pipe / 并发 pipe 雏形
