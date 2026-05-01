#include "pit.h"
#include "process.h"
#include "syscall.h"
#include "vga.h"

// 记录用户态是否已经请求 exit，本轮把它作为一次性测试完成后的收口条件
static int syscall_halt_requested = 0;

// 裸机环境下手动打印无符号整数，便于输出 pid/time
static void syscall_print_uint(unsigned int value) {
    char digits[16];
    int index = 0;

    if (value == 0) {
        print_char('0');
        return;
    }

    while (value > 0) {
        digits[index++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (index > 0) {
        index--;
        print_char(digits[index]);
    }
}

// 根据 eax 分发最小系统调用；当前支持 write/exit/getpid/time
void syscall_handle(struct interrupt_frame* frame) {
    if (frame->eax == SYS_WRITE) {
        print_string((const char*)frame->ebx);
        frame->eax = 0;
        return;
    }

    if (frame->eax == SYS_EXIT) {
        print_string("user exit\n");
        frame->eax = 0;
        process_mark_current_exit();
        syscall_halt_requested = 1;
        return;
    }

    if (frame->eax == SYS_GETPID) {
        frame->eax = (unsigned int)process_current_pid();
        print_string("pid: ");
        syscall_print_uint(frame->eax);
        print_char('\n');
        return;
    }

    if (frame->eax == SYS_TIME) {
        frame->eax = pit_get_ticks();
        print_string("time: ");
        syscall_print_uint(frame->eax);
        print_char('\n');
        return;
    }

    print_string("unknown syscall\n");
    frame->eax = (unsigned int)-1;
}

// 查询当前是否已经收到用户态 exit 请求
int syscall_should_halt(void) {
    return syscall_halt_requested;
}

// 每次进入用户态测试前清理一次状态，避免上轮 exit 影响下一轮
void syscall_clear_halt(void) {
    syscall_halt_requested = 0;
}
