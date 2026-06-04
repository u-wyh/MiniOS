# MiniOS Phase2 Shell 说明

## 1. 当前定位

当前 shell 是教学版最小交互式 shell。

它负责把一行输入拆成：

- 命令名
- 用户程序 `argv`
- 输入重定向
- 输出重定向
- 单个管道左右两侧命令

当前重点不是实现完整 shell 语法，而是让 `run`、`pipe`、`redirect` 和用户态程序链路稳定工作。

## 2. 当前支持的最小语法

支持：

- `run <program> [args...]`
- `start <program> [args...]`
- `run <program> [args...] < file`
- `run <program> [args...] > file`
- `run <program> [args...] >> file`
- `run <program> [args...] < file > file`
- `run A [args...] | run B [args...]`
- `run A [args...] | run B [args...] > file`
- `run A [args...] < file | run B [args...]`

当前不支持：

- 引号
- 转义
- 环境变量
- 多级管道
- 命令替换
- 通配符

## 3. token 切分规则

当前 shell 使用最小空白分隔规则：

- 空格分隔 token
- `tab` 也视为分隔
- 不保留引号语义
- 不保留转义语义

例如：

```text
run head -n 3 < /readme.txt
```

会先切成：

```text
["run", "head", "-n", "3", "<", "/readme.txt"]
```

后续再由 `run` 解析逻辑把 `<` 和 `/readme.txt` 从用户程序 `argv` 中剥离。

## 4. run 命令 argv 生成规则

当前规则是：

- `run` 自己不是用户程序参数
- 程序名会作为用户程序 `argv[0]`
- 普通参数依次成为 `argv[1..]`
- `<`、`>`、`>>`、`|` 以及它们的目标 token 不进入用户程序 `argv`

例如：

```text
run grep MiniOS < /readme.txt
```

会变成：

- program：`grep`
- `argc = 2`
- `argv[0] = "grep"`
- `argv[1] = "MiniOS"`
- `stdin_redirect = "/readme.txt"`

再例如：

```text
run head -n 3 < /readme.txt
```

会变成：

- program：`head`
- `argc = 3`
- `argv[0] = "head"`
- `argv[1] = "-n"`
- `argv[2] = "3"`

## 5. pipe 两侧 argv 规则

当前只支持一个 `|`。

例如：

```text
run cat /readme.txt | run grep MiniOS
```

左侧会变成：

- program：`cat`
- `argv = ["cat", "/readme.txt"]`

右侧会变成：

- program：`grep`
- `argv = ["grep", "MiniOS"]`

例如：

```text
run cat /readme.txt | run head -n 3
```

右侧会变成：

- program：`head`
- `argv = ["head", "-n", "3"]`

## 6. 重定向与 argv 的关系

当前 shell 会先识别：

- `<`
- `>`
- `>>`

再计算“真正传给用户程序的 argc”。

这意味着：

- `< /readme.txt` 不会进入 `argv`
- `> /out.txt` 不会进入 `argv`
- `>> /out.txt` 不会进入 `argv`

Task93 之后，这条规则在普通 `run` 和 pipe 左右两侧都统一复用同一个辅助逻辑。

## 7. 错误输入处理

当前至少保证这些情况不 panic：

- 空输入
- 只有 `run`
- `run grep <`
- `run cat /readme.txt |`
- `| run wc`
- 参数数量超过上限

常见错误提示包括：

- `Usage: run <program>`
- `pipe: missing command`
- `redirect: missing input file`
- `redirect: missing target file`
- `Too many args`

## 8. 与后续任务的关系

Task93 的重点不是做完整 shell，而是把 `argv` 解析和 `pipe/redirect` 下的参数保留整理稳定。

这能为后续 Task94 的 `mini_pipeline` 命令做准备。
