# Task93：Shell argv parser 整理 / 为 mini_pipeline 命令做准备

## 1. 任务目标

本轮不新增完整 shell 语法，而是整理当前 shell 的 `argv` 解析规则，让这些命令在 `pipe/redirect` 下都能稳定保留参数：

- `run grep MiniOS < /readme.txt`
- `run head -n 3 < /readme.txt`
- `run tail -n 3 < /readme.txt`
- `run sort < /readme.txt`
- `run cat /readme.txt | run grep MiniOS`
- `run cat /readme.txt | run head -n 3`

## 2. 为什么现在做这件事

前面已经完成：

- `pipe()`
- `dup2()`
- `fork` 后 fd 继承
- `exec` 后 fd 保留
- `exec argc/argv`
- `pipeline_demo`
- `pipeline_args_demo`

但 shell 如果不能稳定把：

- 程序名
- 普通参数
- `pipe`
- `redirect`

拆成清晰结构，后面就很难继续做更像真实 shell 的 `mini_pipeline` 命令。

## 3. 修改文件

- `kernel/shell_source_parts/parse_helpers.inc`
- `kernel/shell_source_parts/main_loop.inc`
- `readme.md`
- `docs/phase2.md`
- `docs/shell.md`
- `docs/user_programs.md`

## 4. 实现思路

本轮继续沿用最小语法：

- 空格分隔 token
- 不支持引号
- 不支持转义
- 只支持单个 `|`
- 只支持一组 `<` 与一组 `>` / `>>`

核心整理点是：

1. `shell_parse_run_redirects(...)` 仍负责识别 `< / > / >>`
2. 新增统一辅助逻辑，计算“真正传给用户程序的 argc”
3. 确保 `<`、`>`、`>>`、`|` 及其目标 token 不进入用户程序 `argv`
4. 普通 `run` 和 pipe 左右两侧都复用同一条 `effective argc` 规则

## 5. 核心语义

### 普通 run

例如：

```text
run grep MiniOS < /readme.txt
```

会解析为：

- program：`grep`
- `argv[0] = "grep"`
- `argv[1] = "MiniOS"`
- `stdin_redirect = "/readme.txt"`

### run + 参数

例如：

```text
run head -n 3 < /readme.txt
```

会解析为：

- program：`head`
- `argv = ["head", "-n", "3"]`

### pipe 左右两侧

例如：

```text
run cat /readme.txt | run grep MiniOS
```

左侧：

- `argv = ["cat", "/readme.txt"]`

右侧：

- `argv = ["grep", "MiniOS"]`

## 6. 验证命令

建议重点验证：

```text
run grep MiniOS < /readme.txt
run head -n 3 < /readme.txt
run tail -n 3 < /readme.txt
run cat /readme.txt | run grep MiniOS
run cat /readme.txt | run head -n 3
run grep MiniOS < /readme.txt > /grep.txt
run grep MiniOS < /readme.txt >> /grep.txt
run
run grep <
run cat /readme.txt |
| run wc
```

## 7. 当前限制

- 不支持引号
- 不支持转义
- 不支持环境变量
- 不支持多级管道
- 不支持命令替换
- 不支持复杂 shell 语法

## 8. 后续方向

- Task94 可以在当前 parser 基础上继续做 `mini_pipeline` 命令
- 后续也可以继续整理 shell 错误提示与更复杂的 token 语义
