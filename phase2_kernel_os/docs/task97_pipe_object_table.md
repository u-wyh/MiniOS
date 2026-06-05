# Task97：多个 pipe object / pipe 分配表雏形

## 1. 任务目标

把教学版 pipe 从“单全局 pipe buffer”推进到：

```text
pipe_table[MAX_PIPE]
```

让每次 `pipe()` 返回的一对 `FD_PIPE_READ / FD_PIPE_WRITE` 都绑定到一个独立 `pipe_id`，从而避免不同 pipe 之间互相污染。

## 2. 为什么需要多个 pipe object

如果系统里始终只有一个全局 pipe buffer，那么会出现这些问题：

1. 同一时间只能安全使用一个 pipe
2. 连续执行 `pipe_test / pipe_close_test / mini_pipeline` 容易残留状态
3. 不同测试或不同进程看到的其实还是同一份 pipe 数据
4. 后续多级 pipeline 无法自然推进

Task97 的核心变化就是：

1. fd 只保存 `pipe_id`
2. 真正的数据和 open 状态保存在 `pipe_table[pipe_id]`

## 3. 修改文件

本轮主要修改：

1. `include/process.h`
2. `kernel/process.c`
3. `kernel/process_parts/core_helpers.inc`
4. `kernel/process_parts/fd_and_input.inc`
5. `kernel/process_parts/redirect_pipe.inc`
6. `include/user_program.h`
7. `include/fs.h`
8. `kernel/fs_parts/embedded_and_tables.inc`
9. `kernel/user_program_sources/pipe_multi_test_elf_source.c`
10. `kernel/user_program_blobs/pipe_multi_test_elf.inc`
11. `README.md`
12. `docs/phase2.md`
13. `docs/pipe.md`
14. `docs/fd.md`
15. `docs/process.md`
16. `docs/user_programs.md`

## 4. 实现思路

### 4.1 pipe_table

当前新增固定大小的 pipe object 表：

```text
pipe_table[PROCESS_MAX_PIPE_OBJECTS]
```

每个对象维护：

1. `used`
2. `active`
3. `data[PROCESS_PIPE_BUFFER_SIZE]`
4. `read_pos`
5. `write_pos`
6. `count`
7. `read_open`
8. `write_open`

### 4.2 fd 与 pipe_id

当前 `struct process_fd_entry` 新增：

1. `type`
2. `pipe_id`

因此：

1. `FD_PIPE_READ` 表示“这是 pipe 的读端”
2. `FD_PIPE_WRITE` 表示“这是 pipe 的写端”
3. `pipe_id` 表示“它绑定到哪一个 pipe object”

### 4.3 pipe() 分配

当前 `pipe()` syscall 流程是：

1. 从 `pipe_table[]` 找一个空闲对象
2. 初始化该对象
3. 为当前进程分配一个 `FD_PIPE_READ`
4. 为当前进程分配一个 `FD_PIPE_WRITE`
5. 这两个 fd 共用同一个 `pipe_id`

若中途失败，则回滚已分配资源并返回错误。

### 4.4 read / write / close

当前 `pipe_read(fd, ...)` / `pipe_write(fd, ...)` 都会：

1. 从 fd 表项取出 `pipe_id`
2. 根据 `pipe_id` 找到 `pipe_table[pipe_id]`
3. 只操作这个对象自己的 buffer / open 状态

当前 `close(pipe fd)` 也只影响对应 `pipe_id` 的对象：

1. 更新该对象的 `read_open / write_open`
2. 若读写端都关闭，则回收该对象

## 5. pipe table 结构

当前 pipe object 是最小教学版结构，不是完整 POSIX pipe：

1. `used`：该槽位是否已经被占用
2. `active`：该对象是否正在被使用
3. `data[]`：环形缓冲区
4. `read_pos / write_pos / count`：环形缓冲推进状态
5. `read_open / write_open`：当前读端/写端是否仍可见

## 6. fork / dup2 与 pipe_id

当前：

1. `fork()`
   - 子进程复制父进程 `fd_table[]`
   - pipe fd 继承时直接继承 `pipe_id`
2. `dup2()`
   - 复制 pipe fd 时直接复制 `pipe_id`
   - 不会新建 pipe object

这仍然不是完整引用计数模型，但已经足够支撑当前教学版实验。

## 7. 验证命令

本轮重点验证命令：

```text
run pipe_multi_test
run pipe_test
run pipe_close_test
run pipe_fork_dup2_test
run mini_pipeline cat /readme.txt -- wc
run mini_pipeline cat /readme.txt -- grep MiniOS
run mini_pipeline cat /readme.txt -- head -n 3
```

其中 `pipe_multi_test` 用来重点确认：

1. pipe A / pipe B 数据不互相污染
2. close 后 pipe object 可以复用

## 8. 当前限制

当前仍然不支持：

1. 完整 fd 引用计数
2. 完整 POSIX close 语义
3. close-on-exec
4. 多级 Shell pipeline
5. 多个并发 shell pipeline
6. 更复杂的 pipe 生命周期管理

## 9. 后续方向

下一步可以继续推进：

1. Task98：多级 pipeline 数据结构
2. Task99：真正 Shell pipeline 多级解析雏形
3. Task100：Phase2 fd / pipe / process 总结
