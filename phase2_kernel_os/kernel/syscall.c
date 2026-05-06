// syscall.c：实现最小系统调用分发，并处理 fork/waitpid/exit 返回路径
#include "pit.h"
#include "keyboard.h"
#include "process.h"
#include "syscall.h"
#include "vga.h"

#define DEBUG_SYSCALL 0

// 记录用户态是否已经请求 exit，本轮把它作为一次性测试完成后的收口条件
static int syscall_halt_requested = 0;
// 若 syscall 期间需要把 CPU 直接切换到另一个用户态现场，则在这里登记目标 frame
static struct interrupt_frame* syscall_resume_frame = (struct interrupt_frame*)0;

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

// 仅用于 Task31 调试：打印 waitpid/fork 相关返回值，便于观察父子执行路径
static void syscall_debug_result(const char* tag, unsigned int value) {
#if DEBUG_SYSCALL
    print_string("[syscall] ");
    print_string(tag);
    print_string("=");
    syscall_print_uint(value);
    print_char('\n');
#else
    (void)tag;
    (void)value;
#endif
}

// 根据 eax 分发最小系统调用；当前支持 write/exit/getpid/time/fork/waitpid/exec/read_char/get_argc/get_arg/exec_args
void syscall_handle(struct interrupt_frame* frame) {
    struct interrupt_frame* next_frame;

    if (frame->eax == SYS_WRITE) {
        // 教学版后台任务先不占用前台输出，避免 start 出来的测试程序把 shell 提示符冲乱。
        if (process_current_is_background() != 0) {
            frame->eax = 0;
            return;
        }

        print_string((const char*)frame->ebx);
        frame->eax = 0;
        return;
    }

    if (frame->eax == SYS_EXIT) {
        print_string("user exit\n");
        frame->eax = 0;
        process_exit((int)frame->ebx);
        next_frame = process_resume_after_exit();
        if (next_frame != (struct interrupt_frame*)0) {
            syscall_set_resume_frame(next_frame);
            return;
        }

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

    if (frame->eax == SYS_FORK) {
        int fork_result = process_fork(frame);

        // 防御性检查：父进程路径里 fork 不应返回 0；若出现 0，按失败处理避免把异常 pid 透传给用户态。
        if (fork_result == 0) {
            fork_result = -1;
        }

        frame->eax = (unsigned int)fork_result;
        syscall_debug_result("fork return to parent", frame->eax);
        return;
    }

    if (frame->eax == SYS_WAITPID) {
        int result = process_waitpid_syscall((int)frame->ebx, frame, &next_frame);

        if (result == -4) {
#if DEBUG_SYSCALL
            print_string("[syscall] waitpid blocked\n");
#endif
            syscall_set_resume_frame(next_frame);
            return;
        }

        frame->eax = (unsigned int)result;
        syscall_debug_result("waitpid return", frame->eax);
        return;
    }

    if (frame->eax == SYS_EXEC) {
        int result = process_exec_program((int)frame->ebx, frame);

        if (result == 0) {
#if DEBUG_SYSCALL
            print_string("[syscall] exec replaced image\n");
#endif
            return;
        }

        frame->eax = (unsigned int)result;
        syscall_debug_result("exec return", frame->eax);
        return;
    }

    if (frame->eax == SYS_READ_CHAR) {
        char ch = keyboard_read_char();

        if (ch != 0) {
            frame->eax = (unsigned int)(unsigned char)ch;
            return;
        }

        // 没有输入时阻塞当前用户进程，把 CPU 让给其他 READY 进程，而不是在内核里原地 hlt 死等。
        {
            int result = process_read_char_syscall(frame, &next_frame);

            if (result == -4) {
                syscall_set_resume_frame(next_frame);
                return;
            }
        }

        // 若当前没有其他可运行进程，退化为最小 hlt 等待，避免把 shell 永久挂起。
        frame->eax = (unsigned int)(unsigned char)keyboard_read_char_blocking();
        return;
    }

    if (frame->eax == SYS_GET_ARGC) {
        frame->eax = (unsigned int)process_get_argc();
        return;
    }

    if (frame->eax == SYS_GET_ARG) {
        frame->eax = (unsigned int)process_get_arg((int)frame->ebx, (char*)frame->ecx, (int)frame->edx);
        return;
    }

    if (frame->eax == SYS_EXEC_ARGS) {
        int result = process_exec_program_args((int)frame->ebx, (int)frame->ecx, (const char* const*)frame->edx, frame);

        if (result == 0) {
            return;
        }

        frame->eax = (unsigned int)result;
        syscall_debug_result("exec_args return", frame->eax);
        return;
    }

    if (frame->eax == SYS_PS) {
        // SYS_PS 最小语义：ebx=活动进程序号，ecx=用户缓冲区(struct process_info*)，成功返回0
        frame->eax = (unsigned int)process_get_info_by_index((int)frame->ebx, (struct process_info*)frame->ecx);
        return;
    }

    if (frame->eax == SYS_KILL) {
        frame->eax = (unsigned int)process_kill((int)frame->ebx, -9);
        return;
    }

    if (frame->eax == SYS_WAIT_ANY) {
        // 非阻塞 wait_any：有可回收 zombie 返回 pid；无可回收子进程返回 0
        frame->eax = (unsigned int)process_wait_any();
        return;
    }

    if (frame->eax == SYS_YIELD) {
        int result = process_yield_syscall(frame, &next_frame);

        if (result == -4) {
            syscall_set_resume_frame(next_frame);
            return;
        }

        frame->eax = (unsigned int)result;
        return;
    }

    if (frame->eax == SYS_SLEEP) {
        int result = process_sleep_syscall(frame->ebx, frame, &next_frame);

        if (result == -4) {
            syscall_set_resume_frame(next_frame);
            return;
        }

        frame->eax = (unsigned int)result;
        return;
    }

    if (frame->eax == SYS_SLEEP_PID) {
        frame->eax = (unsigned int)process_sleep_pid((int)frame->ebx, frame->ecx);
        return;
    }

    if (frame->eax == SYS_SET_BACKGROUND) {
        frame->eax = (unsigned int)process_set_background_by_pid((int)frame->ebx, (int)frame->ecx);
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

// 记录 syscall 返回时应恢复到哪个用户态 frame，供汇编中断尾部切换执行主体
void syscall_set_resume_frame(struct interrupt_frame* frame) {
    syscall_resume_frame = frame;
}

// 取出一次待恢复 frame，并立刻清空，避免下一个 syscall 误复用旧切换目标
struct interrupt_frame* syscall_take_resume_frame(void) {
    struct interrupt_frame* frame = syscall_resume_frame;

    syscall_resume_frame = (struct interrupt_frame*)0;
    return frame;
}
