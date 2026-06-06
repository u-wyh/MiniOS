# Task91：exec 参数传递整理 / argv 语义补齐

## 1. 任务目标

Task91 的目标不是重写完整 `execve`，而是把当前教学版 `exec` 的参数传递语义整理清楚，并新增最小验证程序。

本轮聚焦：

1. 明确 `SYS_EXEC_ARGS` 是否支持 `argc / argv`
2. 明确参数数量与长度限制
3. 新增 `exec_args_test`
4. 新增 `exec_args_target`

## 2. 为什么做这一步

后续如果要在用户态组合：

1. `grep MiniOS`
2. `head -n 3`
3. `tail -n 3`

就不能只靠“exec 一个固定 program_id”，还必须保证目标程序能在 `exec` 后拿到自己的最小参数列表。

## 3. 当前参数传递模型

当前教学版参数链路是：

```text
shell argv
  -> SYS_EXEC_ARGS(program_id, argc, argv)
    -> process_copy_user_args()
      -> PCB 暂存 user_argc / user_argv
        -> 新程序通过 SYS_GET_ARGC / SYS_GET_ARG 读取
```

这还不是完整 POSIX `execve(path, argv, envp)`：

1. 没有 `envp`
2. 没有完整用户栈 `argc/argv` ABI
3. 没有复杂引用或内存安全模型

## 4. 修改文件

本轮主要修改：

1. `include/user_program.h`
2. `include/fs.h`
3. `kernel/fs_parts/embedded_and_tables.inc`
4. `kernel/user_program_sources/exec_args_test_elf_source.c`
5. `kernel/user_program_sources/exec_args_target_elf_source.c`
6. `kernel/user_program_blobs/exec_args_test_elf.inc`
7. `kernel/user_program_blobs/exec_args_target_elf.inc`
8. `readme.md`
9. `docs/phase2.md`
10. `docs/process.md`
11. `docs/syscall.md`
12. `docs/user_programs.md`

## 5. 核心语义

当前最小语义是：

1. 支持 `argc`
2. 支持 `argv[0]` 表示程序名
3. 支持普通字符串参数
4. 支持固定参数数量上限
5. 支持固定参数长度上限
6. `exec` 后 fd table 仍然保留

当前统一约束：

1. 最大参数数量：`10`
2. 单个参数最大长度：`32` 字节缓冲区，其中包含结尾 `'\0'`

## 6. 验证程序

`run exec_args_test` 当前验证：

1. `exec_args_test` 启动
2. 调用 `SYS_EXEC_ARGS`
3. 目标程序切换为 `exec_args_target`
4. `exec_args_target` 打印：
   - `argc`
   - `argv[0]`
   - `argv[1] = hello`
   - `argv[2] = MiniOS`
5. 最终输出 `exec_args_target: ok`

## 7. 当前限制

当前仍然不支持：

1. `envp`
2. 完整 POSIX `execve`
3. 复杂用户栈参数布局
4. 引号和转义解析
5. 完整 shell argv parser

## 8. 后续方向

Task91 之后，可以继续往这些方向推进：

1. Task92：让 `pipeline_demo` 支持带参数目标程序
2. Task93：整理 shell argv parser
3. Task94：阻塞 pipe / 并发 pipe 雏形
