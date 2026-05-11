# MiniOS Phase2 用户程序表

## 1. 当前定位

当前 MiniOS Phase2 还没有真实文件系统。  
因此 shell 里输入的：

```text
run hello
```

不是像 Linux 那样去磁盘上查 `/bin/hello`，而是走一条教学版最小链路：

```text
program name -> program_id -> 内置 ELF/blob -> exec -> process
```

Task50 的目标，就是把这条链路里的“程序编号、程序名、程序镜像”收口到统一表里，避免 shell 和内核各自维护一套不一致的映射。

## 2. program_id 表

当前统一 program_id 如下：

- `PROGRAM_EXEC_CHILD = 1`
- `PROGRAM_SHELL = 2`
- `PROGRAM_HELLO = 3`
- `PROGRAM_ECHO = 4`
- `PROGRAM_LOOP = 5`
- `PROGRAM_SLEEP_TEST = 6`
- `PROGRAM_INIT = 7`
- `PROGRAM_LOOP_EXIT = 8`
- `PROGRAM_INFO = 9`
- `PROGRAM_FORK = 10`
- `PROGRAM_FORKEXEC = 11`

其中：

- `PROGRAM_INVALID = 0` 表示非法编号
- `PROGRAM_COUNT` 只用于边界检查，不代表真实程序项

## 3. 程序名映射

当前规范程序名映射为：

- `init -> PROGRAM_INIT`
- `shell -> PROGRAM_SHELL`
- `hello -> PROGRAM_HELLO`
- `echo -> PROGRAM_ECHO`
- `loop -> PROGRAM_LOOP`
- `loop_exit -> PROGRAM_LOOP_EXIT`
- `sleep_test -> PROGRAM_SLEEP_TEST`
- `execchild -> PROGRAM_EXEC_CHILD`
- `info -> PROGRAM_INFO`
- `fork -> PROGRAM_FORK`
- `forkexec -> PROGRAM_FORKEXEC`

其中 shell 面向用户默认暴露的可运行程序主要是：

- `hello`
- `echo`
- `loop`
- `loop_exit`
- `sleep_test`

另外为了兼容不方便输入下划线的场景，shell 仍保留：

- `sleeptest -> sleep_test`
- `loopexit -> loop_exit`

这个别名只影响 shell 输入习惯，不改变规范 program name。

## 4. exec 语义

当前教学版 `exec` 的核心语义是：

1. 用户态 shell 先把程序名解析成 `program_id`
2. `SYS_EXEC_ARGS` 把 `program_id` 和最小 `argv` 交给内核
3. 内核通过统一用户程序表找到对应描述符
4. 描述符再关联到内置 ELF/blob
5. 内核把该镜像装入当前进程地址空间，完成最小 exec 替换

也就是说，Task50 之后，`process_exec_program_args()` 不再自己维护另一套 `if program_id == ...` 的名字映射，而是统一复用 `user_program` 模块。

## 5. shell run/start 语义

当前 shell 的 `run/start` 语义仍保持最小教学风格：

- `run <program>`：前台执行，shell 会 `fork` 子进程，再让子进程 `exec` 到目标程序，父进程 `waitpid`
- `start <program>`：后台执行，shell 会 `fork` 子进程并 `exec`，但父进程不等待，立即返回提示符

Task50 后，shell 不再手写一组散落的程序编号，而是通过共享程序清单做 `name -> program_id` 解析。

## 6. 当前限制

当前实现仍有这些明确限制：

1. 暂不支持磁盘文件系统
2. 暂不支持 `PATH` 搜索
3. 暂不支持动态加载外部 ELF 文件
4. 暂不支持 `envp`
5. 内置程序镜像仍由内核预先编译并嵌入

后续如果进入 Phase3，可以在保留 `program_id` 教学模型的基础上，逐步扩展到更真实的文件系统、路径查找和通用 `exec`。
