# Task95：mini_pipeline 支持左侧参数 / 双端 argv 管道命令

## 1. 任务目标

在不改 shell parser、不实现真正 `|` 语法的前提下，增强 `mini_pipeline`，让它支持：

```text
run mini_pipeline <left_prog> [left_args...] -- <right_prog> [right_args...]
```

重点是把 `--` 左右两侧都切分成独立的 `argc / argv`，再分别交给两侧 `exec`。

## 2. 为什么做这个任务

Task94 的 `mini_pipeline` 已经能做固定格式的用户态 pipeline，但左侧只支持一个程序名，还不能表达：

```text
run mini_pipeline cat /readme.txt -- grep MiniOS
```

Task95 的意义就是把 `mini_pipeline` 从“固定 writer demo”推进到“左右两侧都能带参数”的更通用教学版命令入口。

## 3. 修改文件

1. `kernel/user_program_sources/mini_pipeline_elf_source.c`
2. `readme.md`
3. `docs/phase2.md`
4. `docs/user_programs.md`
5. `docs/pipe.md`
6. `docs/process.md`
7. `docs/fd.md`

## 4. 实现思路

1. 在 `mini_pipeline` 内部扫描 `argv`，找到 `--` 分隔符。
2. 用固定数组保存左侧参数和右侧参数。
3. 左侧区间生成 `left_argc / left_argv`。
4. 右侧区间生成 `right_argc / right_argv`。
5. 左侧子进程 `dup2(fds[1], 1)` 后执行 `exec(left_prog, left_argv)`。
6. 右侧子进程 `dup2(fds[0], 0)` 后执行 `exec(right_prog, right_argv)`。
7. 继续沿用教学版顺序 pipe：先运行左侧，再运行右侧。

## 5. 核心语义

当前 `mini_pipeline` 支持：

```text
run mini_pipeline pipeline_writer -- grep MiniOS
run mini_pipeline cat /readme.txt -- grep MiniOS
run mini_pipeline cat /readme.txt -- head -n 3
run mini_pipeline cat /readme.txt -- wc
run mini_pipeline cat /readme.txt -- sort
```

其中：

1. `left_argv[0]` 是左侧程序名。
2. `right_argv[0]` 是右侧程序名。
3. 左侧参数不会传给右侧。
4. 右侧参数不会传给左侧。
5. `--` 不会进入任一侧 `argv`。

## 6. 当前限制

1. 不是完整 shell pipeline。
2. 不支持真正 `|` 语法。
3. 不支持多级管道。
4. 不支持引号与转义。
5. 不支持环境变量。
6. 仍然只有一个全局教学版 pipe buffer。
7. 仍然采用顺序执行，不是并发阻塞 pipe。

## 7. 验证命令

建议验证：

```text
run mini_pipeline pipeline_writer -- grep MiniOS
run mini_pipeline pipeline_writer -- head -n 2
run mini_pipeline cat /readme.txt -- grep MiniOS
run mini_pipeline cat /readme.txt -- head -n 3
run mini_pipeline cat /readme.txt -- wc
run mini_pipeline cat /readme.txt -- sort
```

以及错误路径：

```text
run mini_pipeline
run mini_pipeline cat /readme.txt
run mini_pipeline -- grep MiniOS
run mini_pipeline cat /readme.txt --
```

## 8. 后续方向

1. 继续做更接近真实 shell 的 pipeline 语法。
2. 引入阻塞 pipe / 并发 pipe。
3. 从单全局 pipe buffer 逐步过渡到多个独立 pipe object。
