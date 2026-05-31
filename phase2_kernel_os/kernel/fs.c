// fs.c：文件系统顶层入口。
// 当前保留单一编译入口，但把实现按职责拆到更小的片段文件中，便于阅读与后续继续演进。
#include "fs.h"
#include "vga.h"

/*
 * 组织说明：
 * 1. embedded_and_tables.inc：编译期嵌入的 ELF 镜像，以及 file_table / builtin_text_files / ramfs_files。
 * 2. helpers.inc：最小字符串、路径标准化等文件系统内部辅助函数。
 * 3. ramfs_slots.inc：RAMFS 槽位查找、计数和清理。
 * 4. lookups.inc：程序镜像与内置只读文本文件查询。
 * 5. visible_files.inc：面向 ls/stat 的统一“可见文件”接口。
 * 6. text_io.inc：文本读取、覆盖写、追加写、创建和删除等核心能力。
 * 7. compat.inc：历史兼容/调试辅助接口。
 */

#include "fs_parts/embedded_and_tables.inc"
#include "fs_parts/helpers.inc"
#include "fs_parts/ramfs_slots.inc"
#include "fs_parts/lookups.inc"
#include "fs_parts/visible_files.inc"
#include "fs_parts/text_io.inc"
#include "fs_parts/compat.inc"
