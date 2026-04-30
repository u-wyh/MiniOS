# Task9：内核控制命令扩展

## 1. 本任务目标

本轮让 Kernel Monitor 开始控制真实内核功能，而不只是做字符串输入与简单回显。

## 2. 核心知识点

- 什么是 Kernel Monitor：内核早期阶段的最小控制台入口，用于观察状态和执行诊断命令
- 为什么内核早期需要控制台命令：很多子系统还没完善时，控制台是最直接的调试与验证手段
- tick 计数代表什么：它表示 PIT 定时中断发生了多少次，是系统运行节拍，不是墙上时间
- panic 在内核中的作用：表示发生了不可恢复错误，系统应立即停止后续正常执行
- 为什么裸机环境不能用 `printf`：当前是 freestanding 内核，没有标准库格式化输出支持
- 为什么 panic 后要 `cli + hlt`：`cli` 关闭可屏蔽中断，`hlt` 让 CPU 停在安全状态，避免 panic 后继续乱跑

## 3. 执行流程

键盘输入 ->
`input_buffer` ->
`shell_execute` ->
命令分发 ->
调用 `about` / `pit_get_ticks` / `kernel_panic`

## 4. 关键代码解释

- `pit_get_ticks`：向其他模块暴露真实的系统 tick 计数
- `print_uint`：最小整数输出函数，用于在没有 `printf` 的环境里打印 tick 数值
- `kernel_panic`：输出 panic 信息后执行 `cli` 并进入无限 `hlt` 循环
- `shell_execute` 中新增命令分支：根据输入进入 `about`、`tick`、`panic` 不同处理路径

## 5. 当前限制

- tick 只是系统时钟计数，不是墙上时间
- panic 只是最小停止机制
- 还没有任务系统
- 还没有内存管理
- 控制台命令仍然不支持参数解析

## 6. 常见错误

- tick 写死数字，不能反映 PIT
- panic 后没有 `cli`，仍可能响应中断
- panic 后继续返回 shell，失去 panic 意义
- 使用 `printf` 导致链接失败
- help 忘记更新

## 7. 一句话总结

Task9 让 MiniOS 的控制台开始具备观察和控制内核状态的能力。
