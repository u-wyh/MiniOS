// syscall.c：实现最小系统调用分发，并处理 fork/waitpid/exit 返回路径
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

// 最小字符串相等判断：仅供 syscall 层识别用户态 shell 输出的 "shell exit\n" 标记。
static int syscall_string_equals(const char* a, const char* b) {
    if (a == (const char*)0 || b == (const char*)0) {
        return 0;
    }

    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }

    return (*a == '\0' && *b == '\0') ? 1 : 0;
}

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

// 根据 eax 分发最小系统调用。
// 当前 ABI 约定：
// - eax：syscall 编号
// - ebx/ecx/edx：前 1~3 个参数
// - eax：返回值
void syscall_handle(struct interrupt_frame* frame) {
    struct interrupt_frame* next_frame;

    // SYS_WRITE(text)：ebx=用户态字符串指针，成功返回 0。
    if (frame->eax == SYS_WRITE) {
        const char* text = (const char*)frame->ebx;

        // 识别用户态 shell 的显式 exit 提示，把它记为“用户主动退出”，供 init 决定是否自动拉起新 shell。
        if (syscall_string_equals(text, "shell exit\n") != 0) {
            process_mark_current_requested_exit();
        }

        // 教学版后台任务先不占用前台输出，避免 start 出来的测试程序把 shell 提示符冲乱。
        if (process_current_is_background() != 0) {
            frame->eax = 0;
            return;
        }

        print_string(text);
        frame->eax = 0;
        return;
    }

    // SYS_EXIT(status)：ebx=退出码；当前不会返回到原用户进程。
    if (frame->eax == SYS_EXIT) {
        print_string("user exit\n");
        frame->eax = 0;
        process_exit((int)frame->ebx);
        next_frame = process_resume_after_exit();
        if (next_frame != (struct interrupt_frame*)0) {
            syscall_set_resume_frame(next_frame);
            return;
        }

        // 没有父进程需要立刻恢复时，退出进程已经变成 ZOMBIE。
        // 这里退到 idle 等待后续键盘/PIT 唤醒其他用户进程，避免后台进程退出时掉回内核 shell。
        syscall_idle_requested = 1;
        return;
    }

    // SYS_GETPID()：无参数，返回当前进程 pid。
    if (frame->eax == SYS_GETPID) {
        frame->eax = (unsigned int)process_current_pid();
        print_string("pid: ");
        syscall_print_uint(frame->eax);
        print_char('\n');
        return;
    }

    // SYS_TIME()：历史教学接口，无参数，返回当前 tick。
    if (frame->eax == SYS_TIME) {
        frame->eax = pit_get_ticks();
        print_string("time: ");
        syscall_print_uint(frame->eax);
        print_char('\n');
        return;
    }

    // SYS_FORK()：无参数；父进程返回 child_pid，子进程恢复时看到 0。
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

    // SYS_WAITPID(pid)：ebx=目标子进程 pid；成功返回回收/等待完成的 pid，失败返回负值。
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

    // SYS_EXEC(program_id)：ebx=目标 program_id；成功后直接替换当前镜像，不回到旧程序。
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

    // SYS_READ_CHAR()：无参数；读到字符返回 ASCII，必要时阻塞并切换到其他进程。
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

    // SYS_GET_ARGC()：无参数，返回当前进程保存的教学版 argc。
    if (frame->eax == SYS_GET_ARGC) {
        frame->eax = (unsigned int)process_get_argc();
        return;
    }

    // SYS_GET_ARG(index, buf, max_len)：ebx=参数下标，ecx=用户缓冲区，edx=缓冲区长度。
    if (frame->eax == SYS_GET_ARG) {
        frame->eax = (unsigned int)process_get_arg((int)frame->ebx, (char*)frame->ecx, (int)frame->edx);
        return;
    }

    // SYS_EXEC_ARGS(program_id, argc, argv)：ebx=program_id，ecx=argc，edx=argv 指针。
    if (frame->eax == SYS_EXEC_ARGS) {
        int result = process_exec_program_args((int)frame->ebx, (int)frame->ecx, (const char* const*)frame->edx, frame);

        if (result == 0) {
            return;
        }

        frame->eax = (unsigned int)result;
        syscall_debug_result("exec_args return", frame->eax);
        return;
    }

    // SYS_PS(index, out)：ebx=活动进程序号，ecx=用户态 struct process_info*；成功返回 0。
    if (frame->eax == SYS_PS) {
        frame->eax = (unsigned int)process_get_info_by_index((int)frame->ebx, (struct process_info*)frame->ecx);
        return;
    }

    // SYS_KILL(pid)：ebx=目标 pid；成功返回 0，失败返回负值。
    if (frame->eax == SYS_KILL) {
        // 当前 kill 不是信号系统；PROCESS_KILL_EXIT_STATUS 只是教学版“被 kill”退出状态。
        frame->eax = (unsigned int)process_kill((int)frame->ebx, PROCESS_KILL_EXIT_STATUS);
        return;
    }

    // SYS_WAIT_ANY()：无参数；回收任意一个 zombie 子进程，有结果返回 pid，无结果返回 0。
    if (frame->eax == SYS_WAIT_ANY) {
        frame->eax = (unsigned int)process_wait_any();
        return;
    }

    // SYS_YIELD()：无参数；当前进程主动让出 CPU，成功通常返回 0 或经过切换后恢复。
    if (frame->eax == SYS_YIELD) {
        int result = process_yield_syscall(frame, &next_frame);

        if (result == -4) {
            syscall_set_resume_frame(next_frame);
            return;
        }

        frame->eax = (unsigned int)result;
        return;
    }

    // SYS_SLEEP(ticks)：ebx=睡眠 tick 数；成功返回 0，必要时切到 idle/hlt 等待唤醒。
    if (frame->eax == SYS_SLEEP) {
        int result = process_sleep_syscall(frame->ebx, frame, &next_frame);

        if (result == -4) {
            syscall_set_resume_frame(next_frame);
            return;
        }

        // 没有可切换目标时，当前进程已被置为 SLEEPING；这里退回内核 idle 路径，
        // 让 CPU 用 hlt 等待 PIT tick，而不是在 shell 或 syscall 里忙等。
        if (result == -5) {
            syscall_idle_requested = 1;
            return;
        }

        frame->eax = (unsigned int)result;
        return;
    }

    // SYS_SLEEP_PID(pid, ticks)：ebx=目标 pid，ecx=睡眠 tick 数；主要用于教学调试。
    if (frame->eax == SYS_SLEEP_PID) {
        frame->eax = (unsigned int)process_sleep_pid((int)frame->ebx, frame->ecx);
        return;
    }

    // SYS_SET_BACKGROUND(pid, flag)：ebx=目标 pid，ecx=后台标记；供 start 命令使用。
    if (frame->eax == SYS_SET_BACKGROUND) {
        frame->eax = (unsigned int)process_set_background_by_pid((int)frame->ebx, (int)frame->ecx);
        return;
    }

    // SYS_GET_TICKS()：无参数，返回自系统启动以来累计的 PIT tick 数。
    if (frame->eax == SYS_GET_TICKS) {
        frame->eax = pit_get_ticks();
        return;
    }

    // SYS_CLEAR_SCREEN()：无参数；最小界面辅助接口，成功返回 0。
    if (frame->eax == SYS_CLEAR_SCREEN) {
        // 用户态 clear 命令只做最小清屏，不改变任何进程状态。
        clear_screen();
        frame->eax = 0;
        return;
    }

    // SYS_OPEN(path)：ebx=用户态路径指针；成功返回 fd，失败返回负值。
    if (frame->eax == SYS_OPEN) {
        frame->eax = (unsigned int)process_open_file((const char*)frame->ebx);
        return;
    }

    // SYS_READ(fd, buf, size)：ebx=fd，ecx=用户缓冲区，edx=读取长度；成功返回字节数，EOF 返回 0。
    if (frame->eax == SYS_READ) {
        frame->eax = (unsigned int)process_read_file((int)frame->ebx, (char*)frame->ecx, (int)frame->edx);
        return;
    }

    // SYS_CLOSE(fd)：ebx=fd；成功返回 0，失败返回负值。
    if (frame->eax == SYS_CLOSE) {
        frame->eax = (unsigned int)process_close_file((int)frame->ebx);
        return;
    }

    // SYS_FILE_COUNT()：无参数；返回当前内置只读文件数量。
    if (frame->eax == SYS_FILE_COUNT) {
        frame->eax = fs_builtin_file_count();
        return;
    }

    // SYS_FILE_INFO(index, buf, max_len)：ebx=索引，ecx=用户缓冲区，edx=缓冲区长度；
    // 成功返回文件大小，并把路径复制到 buf；失败返回负值。
    if (frame->eax == SYS_FILE_INFO) {
        frame->eax = (unsigned int)fs_builtin_file_info((int)frame->ebx, (char*)frame->ecx, (int)frame->edx);
        return;
    }

    // 未知 syscall：当前统一返回 -1，并在控制台打印一条最小调试信息。
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

// 查询本次 syscall 是否希望先退回内核 idle/hlt 路径。
int syscall_should_idle(void) {
    return syscall_idle_requested;
}

// 清理一次性的 idle 请求标志，避免后续 syscall 误复用。
void syscall_clear_idle(void) {
    syscall_idle_requested = 0;
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
