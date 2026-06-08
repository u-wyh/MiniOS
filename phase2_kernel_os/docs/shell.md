# MiniOS Phase2 Shell 说明

## 1. 当前定位

当前 shell 是教学版最小交互式 shell。

它负责把一行输入拆成：

- 命令名
- 用户程序 `argv`
- 输入重定向
- 输出重定向
- 多级管道各段命令

当前重点不是实现完整 shell 语法，而是让用户程序、`pipe`、`redirect` 和 `start` 链路稳定工作。

## 2. 当前支持的最小语法

支持：

- `<program> [args...]`
- `start <program> [args...]`
- `<program> [args...] < file`
- `<program> [args...] > file`
- `<program> [args...] >> file`
- `<program> [args...] < file > file`
- `A [args...] | B [args...]`
- `A [args...] | B [args...] | C [args...]`
- `A [args...] | B [args...] > file`
- `A [args...] < file | B [args...]`
- `A [args...] < file | B [args...] | C [args...] > file`

当前不支持：

- 引号
- 转义
- 环境变量
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
head -n 3 < /readme.txt
```

会先切成：

```text
["head", "-n", "3", "<", "/readme.txt"]
```

后续再由用户程序启动路径把 `<` 和 `/readme.txt` 从用户程序 `argv` 中剥离。

## 4. 用户程序 argv 生成规则

当前规则是：

- 程序名会作为用户程序 `argv[0]`
- 普通参数依次成为 `argv[1..]`
- `<`、`>`、`>>`、`|` 以及它们的目标 token 不进入用户程序 `argv`
- 兼容别名 `run <program> ...` 仍可用，但 `run` 自己不会进入用户程序 `argv`

例如：

```text
grep MiniOS < /readme.txt
```

会变成：

- program：`grep`
- `argc = 2`
- `argv[0] = "grep"`
- `argv[1] = "MiniOS"`
- `stdin_redirect = "/readme.txt"`

再例如：

```text
head -n 3 < /readme.txt
```

会变成：

- program：`head`
- `argc = 3`
- `argv[0] = "head"`
- `argv[1] = "-n"`
- `argv[2] = "3"`

## 5. pipe 各段 argv 规则

当前支持多级 `|`，每一段默认直接写程序名；兼容写法 `run ... | run ...` 也仍可用。

例如：

```text
cat /readme.txt | grep MiniOS | wc
```

第一段会变成：

- program：`cat`
- `argv = ["cat", "/readme.txt"]`

第二段会变成：

- program：`grep`
- `argv = ["grep", "MiniOS"]`

第三段会变成：

- program：`wc`
- `argv = ["wc"]`

例如：

```text
cat /readme.txt | head -n 3
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

Task99 之后，这条规则在普通用户程序命令和多级 pipe 各段都统一复用同一个辅助逻辑。

另外当前只允许：

- 首段使用 `< file`
- 末段使用 `> file`
- 末段使用 `>> file`

中间命令段如果带 `<` / `>` / `>>`，shell 会直接报错，不会把这些 token 混进用户程序 `argv`。

## 7. 错误输入处理

当前至少保证这些情况不 panic：

- 空输入
- 只有 `run`
- `grep <`
- `cat /readme.txt |`
- `| wc`
- `cat /readme.txt | grep > /x | wc`
- `cat /readme.txt | grep < /x`
- 参数数量超过上限

常见错误提示包括：

- `Usage: run <program>`
- `pipe: missing command`
- `redirect: missing input file`
- `redirect: missing target file`
- `Too many args`

## 8. 与后续任务的关系

Task99 之后，shell 已经能把多级 `A | B | C` 翻译成一次用户态 `mini_pipeline` 调用；兼容写法 `run A | run B | run C` 仍可继续使用。

也就是说，当前真正执行多级管道的还是用户态 `mini_pipeline`，shell 负责：

- 切出每一段 `argv`
- 剥离首段输入重定向和末段输出重定向
- 把 `|` 转成 `mini_pipeline` 能识别的 `--`

## 9. 交叉阅读

如果想先看 Phase2 全局主线，再回来看 shell 细节，建议配合：

1. [phase2_summary.md](/home/wyh/MiniOS/phase2_kernel_os/docs/phase2_summary.md)
2. [process.md](/home/wyh/MiniOS/phase2_kernel_os/docs/process.md)
3. [pipe.md](/home/wyh/MiniOS/phase2_kernel_os/docs/pipe.md)
4. [phase3_plan.md](/home/wyh/MiniOS/phase2_kernel_os/docs/phase3_plan.md)
