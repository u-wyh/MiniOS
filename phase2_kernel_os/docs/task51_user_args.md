# Task51：用户程序参数传递整理 / argc argv 语义统一

## 1. 本轮目标

- 整理 shell `run/start` 到内核 `exec` 的参数传递链路。
- 明确 `program name`、`argc`、`argv` 的当前教学版语义。
- 收口参数数量和长度边界，避免静默截断与越界复制。

## 2. 为什么需要本任务

Task50 已经统一了 `program name -> program_id -> 内置镜像` 的映射。

接下来需要继续把：

```text
run echo hello minios
```

这类命令的“程序名”和“用户参数”关系整理清楚，否则 shell、exec 和用户程序对同一行文本的理解会继续分散。

## 3. 本轮修改内容

- 统一了参数限制常量：
  - 最多 `8` 个程序参数（包含 `argv[0]` 程序名）
  - 每个参数最多 `31` 个可见字符，额外保留结尾 `'\0'`
- shell 在 `fork/exec` 前增加参数校验：
  - 参数过多报 `Too many args`
  - 参数过长报 `Arg too long`
- 继续保留当前最小语义：
  - `argv[0]` 保存程序名
  - 剩余 token 作为用户参数
- 内核 `process_copy_user_args()` 继续作为兜底，确保 PCB 参数暂存区不会被越界写坏。

## 4. 参数传递链路

当前 MiniOS Phase2 仍采用教学版简化设计：

```text
shell token
    -> program name
        -> program_id
            -> SYS_EXEC_ARGS
                -> process_copy_user_args()
                    -> PCB 暂存 argc/argv
                        -> 用户程序通过 get_argc/get_arg 读取
```

对命令：

```text
run echo hello minios
```

当前语义是：

- shell 命令名：`run`
- 目标程序名：`echo`
- 用户参数：`hello`、`minios`
- 被执行程序看到的参数：
  - `argv[0] = "echo"`
  - `argv[1] = "hello"`
  - `argv[2] = "minios"`

## 5. 验证方式

- `run`：验证缺少程序名时能安全报错
- `run hello`：验证无额外参数程序仍可正常启动
- `run echo`：验证空参数路径
- `run echo hello`：验证单参数传递
- `run echo hello minios phase2`：验证多参数和顺序保持
- 超长参数：验证 `Arg too long`
- 超多参数：验证 `Too many args`
- `start loop`：验证后台启动未被参数整理破坏
- `run loop_exit` / `run sleep_test`：验证其他内置程序路径保持兼容

## 6. 当前限制

- 当前 `argv` 仍暂存在 PCB 中，不是真实用户栈 `argc/argv` ABI
- 暂不支持 `envp`
- 暂不支持 `PATH`
- 暂不支持复杂引号和转义
- 暂不支持从磁盘加载外部 ELF

## 7. 后续任务

- 若后续继续向真实 `execve` 语义演进，可把参数字符串和 `argv[]` 指针迁移到用户栈布局。
- 后续可以继续补充：
  - `envp`
  - 更清晰的用户程序入口 ABI
  - 更接近真实 shell 的引号和转义支持
