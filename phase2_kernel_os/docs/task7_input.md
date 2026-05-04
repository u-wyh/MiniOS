# Task7：输入缓冲区与行输入

## 1. 本任务目标

实现从“字符输入”到“字符串输入”的升级，让 MiniOS 能把多次按键先暂存在缓冲区里，按下 Enter 后再作为一整行输出。

## 2. 核心知识点

- 输入缓冲区（input buffer）：一块临时字符数组，用来把零散按键组合成一整行字符串
- 为什么需要 buffer：单个按键只能表示一个字符，但很多交互逻辑都需要“输入一整行后再处理”
- 字符输入 vs 行输入：字符输入是按一下就处理一次，行输入是连续输入多个字符，按 Enter 后统一提交
- Enter 的作用：表示“这一行输入结束了，可以把缓冲区里的内容当作完整字符串处理”
- 字符串结束符 `'\0'`：C 风格字符串必须用 `'\0'` 结尾，否则 `print_string()` 不知道在哪里停止

## 3. 执行流程（重点）

键盘按键 ->
`keyboard_handler` ->
写入 `input_buffer` ->
按下 Enter ->
补 `'\0'` ->
输出字符串

## 4. 关键代码解释

- `input_buffer`：保存当前这一行已经输入的字符
- `input_index`：记录下一个字符应该写到缓冲区的哪个位置
- Enter 处理逻辑：检测到 `0x1C` 后，把当前 buffer 补上 `'\0'`，换行，再调用 `print_string()`
- `print_string`：读取以 `'\0'` 结尾的字符串，并逐字符输出到 VGA

## 5. 常见错误

- 忘记 `'\0'` -> `print_string` 乱码
- buffer 越界 -> 崩溃
- Enter 没处理 -> 无法形成字符串
- 没清空 `index` -> 数据混乱

## 6. 一句话总结

输入缓冲区让 MiniOS 从“按键响应”升级为“命令输入”。

## 7. Task35 补充：为什么用户态还需要 `read_char`

Task7 的输入缓冲区主要服务于内核 shell：键盘中断把字符拼成一行，然后直接把这一行交给内核命令解析逻辑。

Task35 往前推进后，用户态 shell 也需要输入能力，但用户程序不能直接读端口 `0x60`，所以中间必须增加一层：

- 键盘 IRQ 先把字符写进内核环形缓冲区
- 用户态 shell 再通过 `read_char` syscall 向内核取一个字符

这样就把“硬件输入属于内核”和“用户程序也需要输入”这两件事连接起来了。

## 8. Task36 补充：`read_char` 和 `read_line` 有什么区别

`read_char` 是字符级接口：

- 一次只返回一个字符
- 没有输入时返回 `0`
- 它只负责“把字符交给用户态”

`read_line` 则是用户态 shell 自己在上层拼出来的行级逻辑：

- 循环调用 `read_char`
- 把多个字符写入自己的行缓冲区
- 遇到回车后补 `'\0'`
- 最终把一整行命令交给命令分发逻辑

也就是说：

- `read_char` 是内核提供的最小机制
- `read_line` 是 shell 在用户态实现的最小策略

## 9. 为什么行缓冲可以先放在用户态 shell 里

因为当前阶段的目标只是让最小交互式 shell 跑通，不是一次性做完整 TTY。

把行缓冲先放在用户态 shell 里有几个好处：

- 内核只需要提供最小字符输入 syscall
- 命令解析逻辑保持在用户态
- 不需要现在就引入阻塞队列、stdin、文件描述符表

这符合 MiniOS 当前的教学式分层：

- 内核先提供最小能力
- shell 再用这些能力组合出更高层行为

## 10. 当前输入链路还缺什么

Task36 打通的是：

`keyboard IRQ -> kernel buffer -> read_char -> user shell read_line -> command dispatch`

但它还不是完整命令行子系统，仍然缺：

- 完整退格编辑
- 方向键处理
- 历史记录
- 阻塞读与唤醒
- 真正的 TTY / stdin 抽象

所以这一步的意义是：先把用户态“能读一行命令并解释”跑通，而不是把终端一次做完。

## 11. Task37 补充：`read_line` 和 tokenizer 的关系是什么

Task36 解决的是“如何得到一整行命令”。

Task37 往前再走一步，解决的是“拿到这一行之后怎么拆开”。

两者关系可以理解成两层：

- `read_line`：负责把键盘输入收集成一整行字符串
- tokenizer：负责把这行字符串按空格切成多个 token

所以处理顺序是：

`read_char -> read_line -> split_line -> command dispatch`

也就是说，`read_line` 解决输入边界，tokenizer 解决命令结构。

## 12. `argv[0]` 在 shell 命令分发中的作用

当前最小 shell 里，tokenizer 拆出来的第一个 token 就是命令名，也就是最小意义上的 `argv[0]`。

例如：

- `echo hello minios`
- `argv[0] = "echo"`
- `argv[1] = "hello"`
- `argv[2] = "minios"`

然后 shell 只需要先看 `argv[0]`：

- 是 `help` 就走 help 分支
- 是 `echo` 就走 echo 分支
- 是 `run` 就走 run 分支

这就是最小命令分发器的核心。
