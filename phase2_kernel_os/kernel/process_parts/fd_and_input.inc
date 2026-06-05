// 从教学版 pipe read fd 读取数据：空 pipe 且写端仍打开时做教学版等待，写端关闭且无数据时返回 EOF。
static int process_read_pipe_fd(struct process* proc, int fd, char* user_buf, int size) {
    struct process_fd_entry* entry;
    struct process_pipe_object* pipe;

    if (proc == (struct process*)0 || user_buf == (char*)0) {
        return -1;
    }

    entry = process_fd_get_entry(proc, fd);
    if (entry == (struct process_fd_entry*)0) {
        return -2;
    }

    if (entry->used == 0 || entry->type != PROCESS_FD_TYPE_PIPE_READ) {
        return -3;
    }

    pipe = process_pipe_get(entry->pipe_id);
    if (pipe == (struct process_pipe_object*)0) {
        return -4;
    }

    for (;;) {
        int copy_result;

        copy_result = process_pipe_copy_to_user(pipe, user_buf, size);
        if (copy_result > 0) {
            return copy_result;
        }

        if (pipe->write_open == 0) {
            return 0;
        }

        process_pipe_wait_briefly();
    }
}

// 向教学版 pipe write fd 写入数据：缓冲区满时做教学版等待，直到 reader 读走数据或检测到读端关闭。
static int process_write_pipe_fd(struct process* proc, int fd, const char* user_buf, int size) {
    struct process_fd_entry* entry;
    struct process_pipe_object* pipe;

    if (proc == (struct process*)0 || user_buf == (const char*)0) {
        return -1;
    }

    entry = process_fd_get_entry(proc, fd);
    if (entry == (struct process_fd_entry*)0) {
        return -2;
    }

    if (entry->used == 0 || entry->type != PROCESS_FD_TYPE_PIPE_WRITE) {
        return -3;
    }

    if (size <= 0) {
        return size;
    }

    pipe = process_pipe_get(entry->pipe_id);
    if (pipe == (struct process_pipe_object*)0) {
        return -4;
    }

    return process_pipe_append_from_user(pipe, user_buf, size);
}

// 为指定进程打开一个当前可见文本文件：返回绑定在该进程 fd_table 里的教学版文件 fd。
static int process_open_file_for_proc(struct process* proc, const char* path) {
    struct minios_stat st;
    int i;
    int j;

    if (proc == (struct process*)0 || path == (const char*)0 || path[0] == '\0') {
        return -1;
    }

    if (fs_builtin_file_stat(path, &st) < 0) {
        return -2;
    }

    i = process_alloc_fd_slot(proc);
    if (i >= 0) {
        proc->fd_table[i].used = 1;
        proc->fd_table[i].type = PROCESS_FD_TYPE_FILE;
        proc->fd_table[i].pipe_id = -1;
        for (j = 0; j < MAX_FS_PATH_LEN - 1; j++) {
            proc->fd_table[i].path[j] = path[j];
            if (path[j] == '\0') {
                break;
            }
        }
        if (j == MAX_FS_PATH_LEN - 1 && path[j] != '\0') {
            process_fd_reset_slot(proc, i);
            return -4;
        }
        proc->fd_table[i].path[MAX_FS_PATH_LEN - 1] = '\0';
        proc->fd_table[i].can_write = 0;
        proc->fd_table[i].offset = 0;
        return PROCESS_FD_BASE + i;
    }

    return -3;
}

// 打开一个当前可见文本文件：path -> 当前进程 fd 表项；当前既支持只读内置文件，也支持 RAMFS 文件。
int process_open_file(const char* path) {
    return process_open_file_for_proc(current_process, path);
}

// 为指定进程以写模式打开一个 RAMFS 文本文件：当前只给运行时内存文件分配可写 fd，内置只读文件一律拒绝。
static int process_open_file_write_for_proc(struct process* proc, const char* path) {
    struct minios_stat st;
    int i;
    int j;

    if (proc == (struct process*)0 || path == (const char*)0 || path[0] == '\0') {
        return -1;
    }

    if (fs_builtin_file_stat(path, &st) < 0) {
        return -2;
    }

    if (st.type != MINIOS_FILE_TYPE_RAMFS_TEXT) {
        return -3;
    }

    i = process_alloc_fd_slot(proc);
    if (i >= 0) {
        proc->fd_table[i].used = 1;
        proc->fd_table[i].type = PROCESS_FD_TYPE_FILE;
        proc->fd_table[i].pipe_id = -1;
        for (j = 0; j < MAX_FS_PATH_LEN - 1; j++) {
            proc->fd_table[i].path[j] = path[j];
            if (path[j] == '\0') {
                break;
            }
        }
        if (j == MAX_FS_PATH_LEN - 1 && path[j] != '\0') {
            process_fd_reset_slot(proc, i);
            return -5;
        }
        proc->fd_table[i].path[MAX_FS_PATH_LEN - 1] = '\0';
        proc->fd_table[i].can_write = 1;
        proc->fd_table[i].offset = 0;
        return PROCESS_FD_BASE + i;
    }

    return -4;
}

// 以写模式打开一个 RAMFS 文本文件：当前只给运行时内存文件分配可写 fd，内置只读文件一律拒绝。
int process_open_file_write(const char* path) {
    return process_open_file_write_for_proc(current_process, path);
}

// 从当前进程已打开 fd 读取数据：每次按路径重新解析文件，便于统一支持只读文件和 RAMFS 文件。
int process_read_file(int fd, char* user_buf, int size) {
    struct process* proc = current_process;
    struct process_fd_entry* entry;
    int read_result;

    if (proc == (struct process*)0 || user_buf == (char*)0) {
        return -1;
    }

    if (size < 0) {
        return -2;
    }

    if (size == 0) {
        return 0;
    }

    // 教学版 stdin 重定向：优先支持“从 pipe read fd 读取”，其次才是“从文件读取”。
    // pipe 读取遵循最小 EOF 语义：没有有效数据、尚未启用 pipe，或 read_offset 到达 size 时统一返回 0。
    if (fd == 0) {
        // 兼容入口保留：fd=0 仍然不是完全由 fd_table 统一管理，
        // 但 pipe stdin 现在优先按“stdin 已绑定到 pipe”处理，而不是强依赖原始 oldfd 仍然占着 fd 表槽位。
        if (proc->stdin_redirect_from_pipe != 0) {
            struct process_pipe_object* pipe = process_pipe_get(proc->stdin_pipe_id);

            if (pipe == (struct process_pipe_object*)0) {
                return 0;
            }

            for (;;) {
                int copy_result = process_pipe_copy_to_user(pipe, user_buf, size);

                if (copy_result > 0) {
                    return copy_result;
                }

                if (pipe->write_open == 0) {
                    return 0;
                }

                process_pipe_wait_briefly();
            }
        }

        // 当用户程序读取 fd=0 且 shell 已经配置输入文件时，
        // 继续按保存的 path + offset 从文件读取，而不是尝试做真实 tty/键盘 stdin。
        if (proc->stdin_redirect_enabled == 0) {
            return 0;
        }

        if (proc->stdin_redirect_path[0] == '\0') {
            return -3;
        }

        read_result = fs_read_text_file(proc->stdin_redirect_path, proc->stdin_redirect_offset, user_buf, size);
        if (read_result > 0) {
            proc->stdin_redirect_offset += (uint32_t)read_result;
        }
        return read_result;
    }

    entry = process_fd_get_entry(proc, fd);
    if (entry == (struct process_fd_entry*)0) {
        return -3;
    }

    if (entry->used == 0) {
        return -4;
    }

    if (entry->type == PROCESS_FD_TYPE_PIPE_READ) {
        return process_read_pipe_fd(proc, fd, user_buf, size);
    }

    if (entry->type == PROCESS_FD_TYPE_PIPE_WRITE) {
        return -5;
    }

    if (entry->path[0] == '\0') {
        return -4;
    }

    read_result = fs_read_text_file(entry->path, entry->offset, user_buf, size);
    if (read_result > 0) {
        entry->offset += (uint32_t)read_result;
    }

    return read_result;
}

// 向当前进程已打开的可写 fd 写入数据：当前只支持 RAMFS 文件，写入后 offset 向前推进。
int process_write_file(int fd, const char* user_buf, int size) {
    struct process* proc = current_process;
    struct process_fd_entry* entry;
    int write_result;

    if (proc == (struct process*)0 || user_buf == (const char*)0) {
        return -1;
    }

    if (size < 0) {
        return -2;
    }

    if (size == 0) {
        return 0;
    }

    entry = process_fd_get_entry(proc, fd);
    if (entry == (struct process_fd_entry*)0) {
        return -3;
    }

    if (entry->used == 0) {
        return -4;
    }

    if (entry->type == PROCESS_FD_TYPE_PIPE_READ) {
        return -5;
    }

    if (entry->type == PROCESS_FD_TYPE_PIPE_WRITE) {
        return process_write_pipe_fd(proc, fd, user_buf, size);
    }

    if (entry->path[0] == '\0') {
        return -4;
    }

    if (entry->can_write == 0) {
        return -6;
    }

    write_result = fs_write_text_file(entry->path, entry->offset, user_buf, size);
    if (write_result > 0) {
        entry->offset += (uint32_t)write_result;
    }

    return write_result;
}

// 关闭指定进程里的一个教学版 fd：普通文件 fd 仅清表项，pipe fd 还会更新全局教学版 pipe 端状态。
// 当前没有引用计数：任意 read end / write end 被 close，都会直接把对应全局端标记为关闭。
static int process_close_fd_for_proc(struct process* proc, int fd) {
    struct process_fd_entry* entry;
    struct process_pipe_object* pipe;
    int pipe_id;
    int pipe_type;
    int slot;

    if (proc == (struct process*)0) {
        return -1;
    }

    slot = process_fd_slot_from_number(fd);
    if (slot < 0) {
        return -2;
    }

    entry = &proc->fd_table[slot];
    if (entry->used == 0) {
        return -3;
    }

    pipe_id = entry->pipe_id;
    pipe_type = entry->type;
    process_fd_reset_slot(proc, slot);

    if (pipe_type == PROCESS_FD_TYPE_PIPE_READ || pipe_type == PROCESS_FD_TYPE_PIPE_WRITE) {
        pipe = process_pipe_get(pipe_id);
        if (pipe != (struct process_pipe_object*)0) {
            // 关闭一个具体 fd 之后，只重新扫描该 pipe_id 对应对象的读写端是否还有可见引用。
            // 当前仍然不是完整引用计数模型，但至少不会把别的 pipe object 一起影响掉。
            pipe->read_open = process_pipe_has_read_reference(pipe_id);
            pipe->write_open = process_pipe_has_write_reference(pipe_id);
            process_pipe_try_free(pipe_id);
        }
    }
    return 0;
}

// 进程退出前统一关闭自己 still-open 的教学版 fd；这里只清理当前进程视图，不主动丢弃 pipe buffer 里的旧数据。
static void process_close_all_fds_for_proc(struct process* proc) {
    int stdin_pipe_id;
    int stdout_pipe_id;
    struct process_pipe_object* pipe;
    int i;

    if (proc == (struct process*)0) {
        return;
    }

    for (i = 0; i < PROCESS_MAX_OPEN_FILES; i++) {
        int fd;

        if (proc->fd_table[i].used == 0) {
            continue;
        }

        fd = PROCESS_FD_BASE + i;
        process_close_fd_for_proc(proc, fd);
    }

    stdin_pipe_id = proc->stdin_pipe_id;
    stdout_pipe_id = proc->stdout_pipe_id;

    // 进程整体退出时，标准输入输出上的 pipe 绑定也应一并解除，
    // 避免已经退出的进程仍通过兼容字段“看起来还持有 pipe 引用”。
    proc->stdin_pipe_fd = -1;
    proc->stdout_pipe_fd = -1;
    proc->stdin_pipe_id = -1;
    proc->stdout_pipe_id = -1;
    proc->stdin_redirect_from_pipe = 0;
    proc->stdout_redirect_to_pipe = 0;

    pipe = process_pipe_get(stdin_pipe_id);
    if (pipe != (struct process_pipe_object*)0) {
        pipe->read_open = process_pipe_has_read_reference(stdin_pipe_id);
        pipe->write_open = process_pipe_has_write_reference(stdin_pipe_id);
        process_pipe_try_free(stdin_pipe_id);
    }

    if (stdout_pipe_id >= 0 && stdout_pipe_id != stdin_pipe_id) {
        pipe = process_pipe_get(stdout_pipe_id);
        if (pipe != (struct process_pipe_object*)0) {
            pipe->read_open = process_pipe_has_read_reference(stdout_pipe_id);
            pipe->write_open = process_pipe_has_write_reference(stdout_pipe_id);
            process_pipe_try_free(stdout_pipe_id);
        }
    }
}

// 关闭当前进程的一个教学版 fd：当前既支持普通文件 fd，也支持最小 pipe read/write fd。
int process_close_file(int fd) {
    return process_close_fd_for_proc(current_process, fd);
}

// 用户态 read_char 在没有输入时阻塞当前进程：保存返回现场后切换到其他 READY 进程。
// 这里复用 waiting_pid=-1 作为“等待键盘输入”的最小标记，避免为教学版再引入额外状态字段。
int process_read_char_syscall(struct interrupt_frame* frame, struct interrupt_frame** next_frame) {
    struct process* current = current_process;
    struct process* next;

    if (next_frame != (struct interrupt_frame**)0) {
        *next_frame = (struct interrupt_frame*)0;
    }

    if (current == (struct process*)0 || frame == (struct interrupt_frame*)0) {
        return -1;
    }

    current->saved_frame = *frame;
    current->has_saved_frame = 1;
    current->waiting_pid = -1;
    current->state = PROCESS_BLOCKED;

    next = process_pick_next_ready(current);
    if (next == (struct process*)0 || next == current) {
        current->waiting_pid = 0;
        current->state = PROCESS_RUNNING;
        return -2;
    }

    current_process = next;
    next->state = PROCESS_RUNNING;
    // 读字符阻塞后，只有真正获得 CPU 的进程才增加调度次数。
    process_record_schedule(next);
    process_activate_user_image(next);
    if (next_frame != (struct interrupt_frame**)0) {
        *next_frame = &next->saved_frame;
    }
    return -4;
}

// 键盘到来时把字符直接交给一个阻塞中的 read_char 调用者。
// 这样 shell 等输入时会显示为 BLOCKED，后台进程获得 CPU；收到按键后 shell 再回到 READY。
int process_wake_read_char_waiter(char ch) {
    int i;

    for (i = 0; i < PROCESS_MAX; i++) {
        if (process_table[i].state != PROCESS_BLOCKED) {
            continue;
        }

        if (process_table[i].waiting_pid != -1 || process_table[i].has_saved_frame == 0) {
            continue;
        }

        process_table[i].saved_frame.eax = (unsigned int)(unsigned char)ch;
        process_table[i].waiting_pid = 0;
        process_table[i].state = PROCESS_READY;
        return 1;
    }

    return 0;
}

// 查询是否有用户进程正阻塞等待键盘输入；键盘 IRQ 用它避免把用户 shell 输入误送给内核 shell。
int process_has_read_char_waiter(void) {
    int i;

    for (i = 0; i < PROCESS_MAX; i++) {
        if (process_table[i].state != PROCESS_BLOCKED) {
            continue;
        }

        if (process_table[i].waiting_pid != -1 || process_table[i].has_saved_frame == 0) {
            continue;
        }

        return 1;
    }

    return 0;
}

// 查询当前进程表里是否还有用户进程；只要用户态系统仍存在，键盘输入就不应回落到内核 shell。
int process_has_user_process(void) {
    int i;

    for (i = 0; i < PROCESS_MAX; i++) {
        if (process_table[i].state == PROCESS_UNUSED) {
            continue;
        }

        if (process_table[i].pid <= 0) {
            continue;
        }

        return 1;
    }

    return 0;
}
