// 按 pid 设置后台标记：只允许修改现存进程，供 shell 的 start 命令把子进程标记为后台任务。
int process_set_background_by_pid(int pid, int is_background) {
    struct process* target = process_find_by_pid(pid);

    if (target == (struct process*)0 || target->state == PROCESS_UNUSED) {
        return -1;
    }

    target->is_background = (is_background != 0) ? 1 : 0;
    return 0;
}

// 返回当前进程 pid；没有当前进程时返回 0
int process_current_pid(void) {
    if (current_process == (struct process*)0 || current_process->state != PROCESS_RUNNING) {
        return 0;
    }

    return current_process->pid;
}

// 返回当前进程是否是后台任务：供 syscall 层决定是否把 write 输出到前台控制台
int process_current_is_background(void) {
    if (current_process == (struct process*)0 || current_process->state != PROCESS_RUNNING) {
        return 0;
    }

    return current_process->is_background;
}

// 为指定 PCB 配置教学版 stdout 重定向：> 允许目标缺失，>> 要求目标已存在且必须是 RAMFS 文件。
static int process_set_stdout_redirect(struct process* proc, const char* path, int is_append) {
    struct minios_stat st;
    int stat_result;
    int output_fd;
    int dup_result;
    int output_slot;

    if (proc == (struct process*)0 || path == (const char*)0 || path[0] == '\0') {
        return -1;
    }

    if (process_copy_path(proc->stdout_redirect_path, path, MAX_FS_PATH_LEN) < 0) {
        proc->stdout_redirect_path[0] = '\0';
        return -2;
    }

    stat_result = fs_builtin_file_stat(proc->stdout_redirect_path, &st);
    if (is_append != 0) {
        if (stat_result < 0) {
            proc->stdout_redirect_path[0] = '\0';
            return -3;
        }
        if (st.type != MINIOS_FILE_TYPE_RAMFS_TEXT) {
            proc->stdout_redirect_path[0] = '\0';
            return -4;
        }
    } else if (stat_result == 0 && st.type != MINIOS_FILE_TYPE_RAMFS_TEXT) {
        proc->stdout_redirect_path[0] = '\0';
        return -4;
    }

    // Task82：shell 文件重定向开始优先走“打开文件 fd -> fd_dup2(..., 1)”路径。
    // 当前仍保留 stdout_redirect_* 兼容字段，供 SYS_WRITE(text) 维持原有教学版写文件语义。
    if (stat_result < 0 && is_append == 0) {
        if (fs_create_ramfs_file(proc->stdout_redirect_path) < 0) {
            proc->stdout_redirect_path[0] = '\0';
            return -5;
        }
    }

    output_fd = process_open_file_write_for_proc(proc, proc->stdout_redirect_path);
    if (output_fd < 0) {
        proc->stdout_redirect_path[0] = '\0';
        return -6;
    }

    dup_result = process_fd_dup2(proc, output_fd, 1);
    if (dup_result < 0) {
        output_slot = process_fd_slot_from_number(output_fd);
        process_fd_reset_slot(proc, output_slot);
        proc->stdout_redirect_path[0] = '\0';
        return -7;
    }

    proc->stdout_redirect_append = (is_append != 0) ? 1 : 0;
    proc->stdout_redirect_started = 0;
    output_slot = process_fd_slot_from_number(output_fd);
    process_fd_reset_slot(proc, output_slot);
    return 0;
}

// 为指定 PCB 配置教学版 stdin 重定向：当前只要求目标文件存在，并记录从 offset=0 开始读取。
static int process_set_stdin_redirect(struct process* proc, const char* path) {
    int input_fd;
    int dup_result;
    int input_slot;

    if (proc == (struct process*)0 || path == (const char*)0 || path[0] == '\0') {
        return -1;
    }

    // Task82：shell 输入重定向开始优先走“打开文件 fd -> fd_dup2(..., 0)”路径。
    input_fd = process_open_file_for_proc(proc, path);
    if (input_fd < 0) {
        return -2;
    }

    dup_result = process_fd_dup2(proc, input_fd, 0);
    if (dup_result < 0) {
        input_slot = process_fd_slot_from_number(input_fd);
        process_fd_reset_slot(proc, input_slot);
        return -3;
    }

    input_slot = process_fd_slot_from_number(input_fd);
    process_fd_reset_slot(proc, input_slot);
    return 0;
}

// 为指定 pid 的子进程配置教学版 stdout 重定向；shell 在 fork 成功后、wait 前调用，避免 child/self 设置的竞态问题。
int process_set_stdout_redirect_by_pid(int pid, const char* path, int is_append) {
    struct process* target = process_find_by_pid(pid);

    if (target == (struct process*)0 || target->state == PROCESS_UNUSED) {
        return -1;
    }

    return process_set_stdout_redirect(target, path, is_append);
}

// 为指定 pid 的子进程配置教学版 stdin 重定向；shell 在 fork 成功后、wait 前调用。
int process_set_stdin_redirect_by_pid(int pid, const char* path) {
    struct process* target = process_find_by_pid(pid);

    if (target == (struct process*)0 || target->state == PROCESS_UNUSED) {
        return -1;
    }

    return process_set_stdin_redirect(target, path);
}

// 为指定 pid 的进程启用“stdout -> pipe buffer”模式：当前仅供 shell 顺序执行单管道左侧时使用。
// Task83 之后，这里的核心接线语义是“先分配 pipe write fd，再 fd_dup2(pipe_write_fd, 1)”。
int process_set_stdout_pipe_by_pid(int pid) {
    struct process* target = process_find_by_pid(pid);
    int pipe_fd;
    int dup_result;

    if (target == (struct process*)0 || target->state == PROCESS_UNUSED) {
        return -1;
    }

    pipe_fd = process_alloc_pipe_fd(target, PROCESS_FD_TYPE_PIPE_WRITE);
    if (pipe_fd < 0) {
        return -2;
    }

    process_pipe_buffer.active = 1;
    dup_result = process_fd_dup2(target, pipe_fd, 1);
    if (dup_result < 0) {
        process_fd_reset_slot(target, process_fd_slot_from_number(pipe_fd));
        return -3;
    }
    return 0;
}

// 为指定 pid 的进程启用“stdin <- pipe buffer”模式：当前仅供 shell 顺序执行单管道右侧时使用。
// Task83 之后，这里的核心接线语义是“先分配 pipe read fd，再 fd_dup2(pipe_read_fd, 0)”。
int process_set_stdin_pipe_by_pid(int pid) {
    struct process* target = process_find_by_pid(pid);
    int pipe_fd;
    int dup_result;

    if (target == (struct process*)0 || target->state == PROCESS_UNUSED) {
        return -1;
    }

    pipe_fd = process_alloc_pipe_fd(target, PROCESS_FD_TYPE_PIPE_READ);
    if (pipe_fd < 0) {
        return -2;
    }

    process_pipe_buffer.active = 1;
    process_pipe_buffer.read_offset = 0;
    dup_result = process_fd_dup2(target, pipe_fd, 0);
    if (dup_result < 0) {
        process_fd_reset_slot(target, process_fd_slot_from_number(pipe_fd));
        return -3;
    }
    return 0;
}

// 为指定 pid 的 shell 子进程解除启动门闩：父进程完成重定向/pipe 配置后再把它切回 READY。
int process_mark_launch_ready_by_pid(int pid) {
    struct process* target = process_find_by_pid(pid);

    if (target == (struct process*)0 || target->state == PROCESS_UNUSED) {
        return -1;
    }

    target->launch_ready = 1;
    if (target->state == PROCESS_BLOCKED) {
        target->state = PROCESS_READY;
    }

    return 0;
}

// 返回当前运行进程的启动门闩状态；默认 1 代表“不需要等待”，仅 shell run/start 子分支会主动轮询它。
int process_current_launch_ready(void) {
    if (current_process == (struct process*)0 || current_process->state != PROCESS_RUNNING) {
        return 1;
    }

    return current_process->launch_ready;
}

// 判断当前进程是否启用了 stdout 重定向；供 SYS_WRITE 在屏幕输出与 RAMFS 写入之间分流。
int process_current_has_stdout_redirect(void) {
    if (current_process == (struct process*)0 || current_process->state != PROCESS_RUNNING) {
        return 0;
    }

    return current_process->stdout_redirect_enabled;
}

// 判断当前进程是否启用了 stdout -> pipe：供 SYS_WRITE 把文本优先写入教学版 pipe buffer。
int process_current_has_stdout_pipe(void) {
    struct process_fd_entry* entry;

    if (current_process == (struct process*)0 || current_process->state != PROCESS_RUNNING) {
        return 0;
    }

    if (current_process->stdout_pipe_fd < PROCESS_FD_BASE) {
        return 0;
    }

    entry = process_fd_get_entry(current_process, current_process->stdout_pipe_fd);
    if (entry == (struct process_fd_entry*)0 || entry->used == 0 || entry->type != PROCESS_FD_TYPE_PIPE_WRITE) {
        return 0;
    }

    // 兼容字段 stdout_redirect_to_pipe 暂时保留，供 shell / 旧文档继续表达“stdout 已被 pipe 接管”。
    // 实际写入目标已经优先由 stdout_pipe_fd 指向的 pipe write fd 决定。
    return 1;
}

// 把当前进程的一次 SYS_WRITE 文本输出写到 RAMFS 重定向目标：
// > 第一次写覆盖，其后改为追加；>> 始终追加。
int process_write_stdout_redirect(const char* text) {
    struct process* proc = current_process;
    struct minios_stat st;
    int text_len;
    int write_result;

    if (proc == (struct process*)0 || proc->stdout_redirect_enabled == 0) {
        return -1;
    }

    if (proc->stdout_redirect_path[0] == '\0') {
        return -2;
    }

    text_len = process_text_length(text);
    if (text_len < 0) {
        return -3;
    }

    if (text_len == 0) {
        return 0;
    }

    if (proc->stdout_redirect_started == 0 && proc->stdout_redirect_append == 0) {
        if (fs_builtin_file_stat(proc->stdout_redirect_path, &st) < 0) {
            write_result = fs_create_ramfs_file(proc->stdout_redirect_path);
            if (write_result < 0) {
                return -4;
            }
        }

        write_result = fs_write_text_file(proc->stdout_redirect_path, 0, text, text_len);
        if (write_result > 0) {
            proc->stdout_redirect_started = 1;
        }
        return write_result;
    }

    write_result = fs_append_ramfs_file(proc->stdout_redirect_path, text);
    if (write_result > 0 || text_len == 0) {
        proc->stdout_redirect_started = 1;
    }
    return write_result;
}

// 把当前进程的一次 SYS_WRITE 文本输出追加到教学版 pipe buffer：
// 当前采用“尽量写入剩余空间 + 满时只提示一次”的教学版策略，避免越界和 panic。
int process_write_stdout_pipe(const char* text) {
    struct process* proc = current_process;
    int text_len;

    if (proc == (struct process*)0) {
        return -1;
    }

    text_len = process_text_length(text);
    if (text_len < 0) {
        return -2;
    }

    if (text_len == 0) {
        return 0;
    }

    if (proc->stdout_pipe_fd < PROCESS_FD_BASE) {
        return -3;
    }

    return process_write_pipe_fd(proc, proc->stdout_pipe_fd, text, text_len);
}

// 清空教学版单管道缓冲区：shell 在执行 run A | run B 前后都应调用，避免残留旧数据。
void process_pipe_reset(void) {
    process_pipe_buffer_clear();
}

// 为当前进程创建一对教学版 pipe fd：fds[0] 返回读端，fds[1] 返回写端。
// 当前仍然只复用一个全局 pipe buffer，因此创建新 pipe 时会先 reset 全局状态。
int process_pipe_create_fds(int* user_fds) {
    struct process* proc = current_process;
    int read_fd;
    int write_fd;

    if (proc == (struct process*)0 || user_fds == (int*)0) {
        return -1;
    }

    if (process_user_range_is_accessible(proc, (uint32_t)user_fds, (uint32_t)(sizeof(int) * 2)) == 0) {
        return -2;
    }

    process_pipe_reset();
    process_pipe_buffer.active = 1;

    read_fd = process_alloc_pipe_fd(proc, PROCESS_FD_TYPE_PIPE_READ);
    if (read_fd < 0) {
        process_pipe_reset();
        return -3;
    }

    write_fd = process_alloc_pipe_fd(proc, PROCESS_FD_TYPE_PIPE_WRITE);
    if (write_fd < 0) {
        process_close_file(read_fd);
        process_pipe_reset();
        return -4;
    }

    user_fds[0] = read_fd;
    user_fds[1] = write_fd;
    return 0;
}

// 当前只把用户态 shell 的显式 exit 命令视为“允许 init 自动重启 shell”的正常退出来源。
void process_mark_current_requested_exit(void) {
    if (current_process == (struct process*)0) {
        return;
    }

    current_process->requested_exit = 1;
}
