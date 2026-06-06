# Task22：exec 机制（shell 运行用户程序）

## 1. exec 是什么

`exec` 是内核提供的“按程序名加载并执行用户程序”接口。

在本任务中，`exec(const char* name)` 只支持一个内置程序名：`test`。

## 2. shell 如何调用程序

Shell 新增命令：

- `run test`

当输入该命令时，shell 调用：

- `exec("test")`

## 3. exec 与 ELF Loader 的关系

`exec` 不负责解析 ELF 细节；它负责“按名字选择程序并触发执行”。

真正的 ELF 解析、段加载、映射与入口返回由 `elf_load()` 完成。

职责拆分：

- `exec`：程序选择与执行控制
- `elf_load`：ELF 解析与内存装载

## 4. 执行流程

本轮执行链路：

`shell -> exec -> elf_load -> user mode`

具体步骤：

1. shell 收到 `run test`
2. `exec("test")` 命中内置程序表
3. `exec` 取到内置 test ELF 镜像
4. 调用 `elf_load(elf_image, elf_size)`
5. 获取 `entry` 后调用 `enter_user_mode(entry, USER_STACK_TOP)`
6. 用户程序运行并通过 syscall 输出 `Hello from ELF`

## 5. 限制说明

- 不实现文件系统
- 不实现路径解析
- 不实现多程序管理
- 仅支持一个内置 ELF：`test`
