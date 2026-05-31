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

        // Task69：若当前进程的 stdout 被教学版单管道接管，则把文本写入 pipe buffer，而不是屏幕或 RAMFS 文件。
        if (process_current_has_stdout_pipe() != 0) {
            frame->eax = (unsigned int)process_write_stdout_pipe(text);
            return;
        }

        // Task66：若当前进程启用了 stdout 重定向，则把整段输出写入 RAMFS，而不是直接落到前台屏幕。
        if (process_current_has_stdout_redirect() != 0) {
            frame->eax = (unsigned int)process_write_stdout_redirect(text);
            return;
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
    // 若 fd=0 且当前进程启用了 stdin 重定向，则该读取会改为从目标文件读取。
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

    // SYS_STAT(path, out)：ebx=用户态路径指针，ecx=用户态 minios_stat*；
    // 成功返回 0，并写入 size/type；失败返回负值。
    if (frame->eax == SYS_STAT) {
        frame->eax = (unsigned int)fs_builtin_file_stat((const char*)frame->ebx, (struct minios_stat*)frame->ecx);
        return;
    }

    // SYS_TOUCH(path)：ebx=用户态路径指针；成功返回 0，失败返回负值。
    if (frame->eax == SYS_TOUCH) {
        frame->eax = (unsigned int)fs_create_ramfs_file((const char*)frame->ebx);
        return;
    }

    // SYS_WRITEFILE(path, text)：ebx=用户态路径指针，ecx=用户态文本指针；
    // 成功返回 0，失败返回负值。当前只支持覆盖写入小文本文件。
    if (frame->eax == SYS_WRITEFILE) {
        frame->eax = (unsigned int)fs_write_ramfs_file((const char*)frame->ebx, (const char*)frame->ecx);
        return;
    }

    // SYS_RM(path)：ebx=用户态路径指针；成功返回 0，失败返回负值。
    if (frame->eax == SYS_RM) {
        frame->eax = (unsigned int)fs_remove_ramfs_file((const char*)frame->ebx);
        return;
    }

    // SYS_OPEN_WRITE(path)：ebx=用户态路径指针；成功返回可写 fd，失败返回负值。
    if (frame->eax == SYS_OPEN_WRITE) {
        frame->eax = (unsigned int)process_open_file_write((const char*)frame->ebx);
        return;
    }

    // SYS_FD_WRITE(fd, buf, size)：ebx=fd，ecx=用户缓冲区，edx=写入长度；
    // 成功返回实际写入字节数，失败返回负值。当前只允许写 RAMFS 文件，不改变 stdout SYS_WRITE 语义。
    if (frame->eax == SYS_FD_WRITE) {
        frame->eax = (unsigned int)process_write_file((int)frame->ebx, (const char*)frame->ecx, (int)frame->edx);
        return;
    }

    // SYS_APPEND_FILE(path, text)：ebx=用户态路径指针，ecx=用户态文本指针；
    // 成功返回追加字节数，失败返回负值。当前只支持把文本追加到 RAMFS 文件末尾。
    if (frame->eax == SYS_APPEND_FILE) {
        frame->eax = (unsigned int)fs_append_ramfs_file((const char*)frame->ebx, (const char*)frame->ecx);
        return;
    }

    // SYS_SET_STDOUT_REDIRECT(pid, path, append)：ebx=目标 pid，ecx=目标路径，edx=是否使用 >>；
    // shell 在 fork 成功后调用它，为即将 exec 的子进程设置 stdout 重定向。
    if (frame->eax == SYS_SET_STDOUT_REDIRECT) {
        frame->eax = (unsigned int)process_set_stdout_redirect_by_pid((int)frame->ebx, (const char*)frame->ecx, (int)frame->edx);
        return;
    }

    // SYS_SET_STDIN_REDIRECT(pid, path)：ebx=目标 pid，ecx=输入文件路径；
    // shell 在 fork 成功后调用它，为即将 exec 的子进程设置 stdin 重定向。
    if (frame->eax == SYS_SET_STDIN_REDIRECT) {
        frame->eax = (unsigned int)process_set_stdin_redirect_by_pid((int)frame->ebx, (const char*)frame->ecx);
        return;
    }

    // SYS_PIPE_RESET()：清空教学版单管道缓冲区，供 shell 在执行 run A | run B 前后重置状态。
    if (frame->eax == SYS_PIPE_RESET) {
        process_pipe_reset();
        frame->eax = 0;
        return;
    }

    // SYS_SET_STDOUT_PIPE(pid)：把指定 pid 的 stdout 改为写入教学版 pipe buffer。
    if (frame->eax == SYS_SET_STDOUT_PIPE) {
        frame->eax = (unsigned int)process_set_stdout_pipe_by_pid((int)frame->ebx);
        return;
    }

    // SYS_SET_STDIN_PIPE(pid)：把指定 pid 的 stdin 改为从教学版 pipe buffer 读取。
    if (frame->eax == SYS_SET_STDIN_PIPE) {
        frame->eax = (unsigned int)process_set_stdin_pipe_by_pid((int)frame->ebx);
        return;
    }

    // SYS_SET_LAUNCH_READY(pid)：shell 完成子进程重定向/pipe 配置后，显式放行它开始执行。
    if (frame->eax == SYS_SET_LAUNCH_READY) {
        frame->eax = (unsigned int)process_mark_launch_ready_by_pid((int)frame->ebx);
        return;
    }

    // SYS_GET_LAUNCH_READY()：shell 子分支在 exec 前轮询父进程是否已完成重定向/pipe 配置。
    if (frame->eax == SYS_GET_LAUNCH_READY) {
        frame->eax = (unsigned int)process_current_launch_ready();
        return;
    }

    // 未知 syscall：当前统一返回 -1，并在控制台打印一条最小调试信息。
    print_string("unknown syscall\n");
    frame->eax = (unsigned int)-1;
}
