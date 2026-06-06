# Task37：用户态 Shell 参数解析雏形

## 1. `read_line` 和 tokenizer 的关系是什么？

`read_line` 负责“拿到一整行”，tokenizer 负责“把这一行拆开”。

也就是说：

- `read_line` 解决输入边界
- tokenizer 解决命令结构

在当前 MiniOS 里，顺序是：

1. shell 通过 `read_char` 逐字符拿输入
2. shell 把字符拼成一整行
3. shell 再按空格把这一行拆成多个 token
4. shell 根据第一个 token 决定执行哪个命令

所以 Task37 不是重做输入链路，而是在 Task36 的一整行输入之上，再补一层最小语法拆分。

## 2. `argv[0]` 在 shell 命令分发中的作用是什么？

当前最小 shell 里，tokenizer 拆出来的第一个 token 就是命令名，也就是最小意义上的 `argv[0]`。

例如输入：

`echo hello minios`

拆分后最小效果就是：

- `argv[0] = "echo"`
- `argv[1] = "hello"`
- `argv[2] = "minios"`

然后 shell 先看 `argv[0]`：

- 如果是 `help`，进入 help 分支
- 如果是 `echo`，进入 echo 分支
- 如果是 `run`，进入 run 分支
- 如果都不是，就是未知命令

所以 `argv[0]` 是最小命令分发的入口。

## 3. `echo` 为什么是内建命令？

因为 `echo` 只是把 shell 已经拿到的参数重新打印出来。

它不需要：

- 新建子进程
- 装载新的用户程序
- 替换当前进程镜像

所以最简单、最自然的做法，就是让 shell 自己直接完成它。

这也体现了一个教学上的分层：

- 有些命令只是 shell 自己的行为
- 有些命令才需要启动外部程序

## 4. `run` 为什么需要 `fork / exec / waitpid`？

因为 `run` 的职责不是“让 shell 自己变成某个程序”，而是“让 shell 去启动另一个程序”。

如果 shell 直接 `exec` 成 `hello`：

- shell 本身就没了
- 程序退出后不会自动回到 shell
- 交互循环就断了

所以当前最小正确路径必须是：

1. shell `fork`
2. 子进程 `exec` 成目标程序
3. 父进程 `waitpid`

这样外部程序退出后，shell 仍然存在，可以继续显示提示符和接受命令。

## 5. 当前 `run <program>` 为什么只支持固定内置程序？

因为当前 MiniOS 的 `exec` 还不是按真实路径字符串加载任意 ELF。

当前最小模型仍然是：

- shell 先识别程序名
- 再把它翻译成固定的 `program_id`
- 内核根据这个 id 找到内置用户程序镜像

所以 `run hello` 本质上还是“按固定编号启动一个内置程序”，不是完整文件系统意义上的程序装载。

## 6. 当前参数解析不支持哪些能力？

Task37 故意保持最小实现，只支持：

- 按空格拆分
- 有限数量 token
- 原地把输入字符串切开

还不支持：

- 引号
- 转义
- 环境变量
- PATH 搜索
- 管道
- 重定向
- `argv/envp` 传递给被执行程序

所以它只是“最小命令参数解析”，不是完整 shell 语法。

## 7. 后续要支持真正的 `exec argv/envp` 还需要做什么？

后续如果继续往前推进，至少还需要：

- 路径字符串查找程序
- `exec(path, argv, envp)` 形式的接口
- 在新程序用户栈上布置参数数据
- 父进程把 token 变成真正的 `argv`
- 更完整的文件系统与 stdin/stdout 抽象

Task37 先完成的是更靠前的一步：

先让 shell 有能力把一行命令拆成 token，并区分：

- 这是 shell 自己能直接做的内建命令
- 还是需要通过 `fork/exec/waitpid` 启动的外部程序
