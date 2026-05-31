// syscall.c：系统调用顶层入口。
// 保留单一中断分发入口，同时把小工具、分发表达和一次性状态管理拆到更小片段中。
#include "pit.h"
#include "keyboard.h"
#include "process.h"
#include "syscall.h"
#include "vga.h"

#define DEBUG_SYSCALL 0

// 记录用户态是否已经请求 exit，本轮把它作为一次性测试完成后的收口条件
static int syscall_halt_requested = 0;
// 记录 syscall 是否需要暂时退回内核 idle/hlt 路径，等待后续 PIT/键盘把 READY 进程切回来。
static int syscall_idle_requested = 0;
// 若 syscall 期间需要把 CPU 直接切换到另一个用户态现场，则在这里登记目标 frame
static struct interrupt_frame* syscall_resume_frame = (struct interrupt_frame*)0;

/*
 * 组织说明：
 * 1. helpers.inc：最小字符串/数字输出调试辅助。
 * 2. dispatch.inc：syscall_handle 主分发逻辑。
 * 3. state_flags.inc：halt/idle/resume_frame 一次性状态接口。
 */

#include "syscall_parts/helpers.inc"
#include "syscall_parts/dispatch.inc"
#include "syscall_parts/state_flags.inc"
