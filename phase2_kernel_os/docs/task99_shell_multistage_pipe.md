# Task99：真正 Shell 多级管道解析雏形

## 1. 任务目标

把 shell 从“只支持单个 `|` 的教学版命令”推进到“支持多级 `run ... | run ... | run ...` 的雏形”，同时尽量复用已经完成的：

1. `mini_pipeline`
2. 多 pipe object
3. `fork`
4. `dup2`
5. `exec(argc, argv)`

## 2. 为什么现在做 Task99

到 Task98 为止，MiniOS 已经有了：

1. 多个独立 pipe object
2. 多级 `mini_pipeline`
3. `pipe + fork + dup2 + exec` 的教学版完整数据流

但 shell 原生语法仍然停留在：

```text
run A | run B
```

因此 Task99 的意义是：

1. 把 shell 原生 `|` 语法接到现有多级 pipeline 执行路径
2. 让用户不用手写 `mini_pipeline ... -- ... -- ...`
3. 在不重写 pipe/fd/process 主机制的前提下，获得更接近真实 shell 的使用体验

## 3. 修改文件

本轮主要修改：

1. `kernel/shell_source_parts/parse_helpers.inc`
2. `kernel/shell_source_parts/pipe_exec.inc`
3. `kernel/shell_source_parts/main_loop.inc`
4. `kernel/user_program_blobs/shell_elf.inc`
5. `README.md`
6. `docs/phase2.md`
7. `docs/shell.md`
8. `docs/pipe.md`
9. `docs/process.md`
10. `docs/fd.md`
11. `docs/user_programs.md`

## 4. 实现思路

### 4.1 不重写底层 pipeline，只翻译 shell 语法

Task99 没有直接重写多级 pipe/fork/dup2/exec，而是采用最小迁移策略：

1. shell 负责解析多个 `|`
2. shell 负责切出每一段 `run ...`
3. shell 负责把 `|` 转成 `mini_pipeline` 使用的 `--`
4. shell 再执行一次：

```text
run mini_pipeline ...
```

所以真正跑多级 pipeline 的，仍然是用户态 `mini_pipeline`。

### 4.2 多个 `|` 的统计

新增一个最小辅助逻辑，用于扫描整行命令里的所有 `|` 下标。

例如：

```text
run cat /readme.txt | run grep MiniOS | run wc
```

会得到两个分隔位置，对应三段命令。

### 4.3 每一段都必须显式写 run

当前 shell 多级管道仍保持教学版限制：

```text
run cat /readme.txt | run grep MiniOS | run wc
```

合法，但：

```text
cat /readme.txt | grep MiniOS | wc
```

当前不支持。

### 4.4 argv 切分规则

shell 会把：

```text
run cat /readme.txt | run grep MiniOS | run wc
```

翻译成：

```text
mini_pipeline cat /readme.txt -- grep MiniOS -- wc
```

其中：

1. 每一段开头的 `run` 会被去掉
2. `|` 会变成 `--`
3. 每一段的普通参数会保留
4. `|` 不会进入任何一段的 `argv`

### 4.5 redirect 与 pipeline 的关系

当前支持：

1. 首段 `< input`
2. 末段 `> output`
3. 末段 `>> output`

例如：

```text
run cat < /readme.txt | run grep MiniOS | run wc > /count.txt
```

shell 会：

1. 把首段 `< /readme.txt` 从左侧 `argv` 剥离
2. 把末段 `> /count.txt` 从最后一段 `argv` 剥离
3. 再把剩余命令段翻译给 `mini_pipeline`

当前中间段仍不支持 `<` / `>` / `>>`。

## 5. 当前语义

当前 shell 已支持：

```text
run cat /readme.txt | run grep MiniOS | run wc
run cat /readme.txt | run head -n 5 | run tail -n 2
run cat < /readme.txt | run grep MiniOS | run wc > /count.txt
```

当前多级 shell pipeline 的本质语义是：

1. shell 先解析出 `N` 段命令
2. shell 把它们翻译成一次 `mini_pipeline`
3. `mini_pipeline` 创建 `N-1` 个 pipe
4. `mini_pipeline` 为每一段 fork 一个子进程
5. 每段子进程通过 `dup2(fd=0/1)` 接到正确 pipe 端点
6. 再 `exec(argc, argv)` 执行真正程序

## 6. 验证命令

本轮重点验证：

```text
run cat /readme.txt | run grep MiniOS | run wc
run cat /readme.txt | run head -n 5 | run tail -n 2
run cat < /readme.txt | run grep MiniOS | run wc > /count.txt
run cat /readme.txt | run grep MiniOS
run cat /readme.txt | run head -n 3
```

错误路径至少包括：

```text
run cat /readme.txt |
| run wc
run cat /readme.txt | run grep > /x | run wc
run cat /readme.txt | run grep < /x
```

## 7. 当前限制

当前仍然不支持：

1. 不带 `run` 的原生命令段
2. 中间段重定向
3. 引号
4. 转义
5. 环境变量
6. 通配符
7. 完整 shell job control
8. 完整 POSIX pipeline 生命周期

## 8. 后续方向

下一步可以继续推进：

1. Task100：Phase2 shell/fd/pipe/process 总结文档
2. Task101：交互式回归测试清单
3. 后续如果继续深入，再考虑更完整 shell parser 与真正 `|` 执行路径统一
