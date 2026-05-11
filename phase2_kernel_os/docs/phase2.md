# MiniOS Phase2 进度补充

## Task49：系统调用表整理与文档化

- 整理了当前 syscall 编号分组和命名。
- 补充了 syscall 分发层的参数 / 返回值注释。
- 新增 `docs/syscall.md`，集中记录当前 Phase2 的 syscall ABI。

## Task50：用户程序表 / program_id 整理

- 统一了内置用户程序 `program_id`。
- 统一了 `program name -> program_id` 映射。
- 统一了 `program_id -> ELF/blob` 查询入口。
- shell `run/start` 与内核 `exec` 复用同一份用户程序表。
- 当前仍不支持真实文件系统、`PATH` 搜索和外部 ELF 动态加载。
