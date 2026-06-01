// 打开一个当前可见文本文件：path -> 当前进程 fd 表项；当前既支持只读内置文件，也支持 RAMFS 文件。
int process_open_file(const char* path) {
    struct process* proc = current_process;
    struct minios_stat st;
    int i;
    int j;

    if (proc == (struct process*)0 || path == (const char*)0 || path[0] == '\0') {
        return -1;
    }

    if (fs_builtin_file_stat(path, &st) < 0) {
        return -2;
    }

    for (i = 0; i < PROCESS_MAX_OPEN_FILES; i++) {
        if (proc->fd_table[i].used != 0) {
            continue;
        }

        proc->fd_table[i].used = 1;
        for (j = 0; j < MAX_FS_PATH_LEN - 1; j++) {
            proc->fd_table[i].path[j] = path[j];
            if (path[j] == '\0') {
                break;
            }
        }
        if (j == MAX_FS_PATH_LEN - 1 && path[j] != '\0') {
            proc->fd_table[i].used = 0;
            proc->fd_table[i].path[0] = '\0';
            proc->fd_table[i].can_write = 0;
            return -4;
        }
        proc->fd_table[i].path[MAX_FS_PATH_LEN - 1] = '\0';
        proc->fd_table[i].can_write = 0;
        proc->fd_table[i].offset = 0;
        return PROCESS_FD_BASE + i;
    }

    return -3;
}

// 以写模式打开一个 RAMFS 文本文件：当前只给运行时内存文件分配可写 fd，内置只读文件一律拒绝。
int process_open_file_write(const char* path) {
    struct process* proc = current_process;
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

    for (i = 0; i < PROCESS_MAX_OPEN_FILES; i++) {
        if (proc->fd_table[i].used != 0) {
            continue;
        }

        proc->fd_table[i].used = 1;
        for (j = 0; j < MAX_FS_PATH_LEN - 1; j++) {
            proc->fd_table[i].path[j] = path[j];
            if (path[j] == '\0') {
                break;
            }
        }
        if (j == MAX_FS_PATH_LEN - 1 && path[j] != '\0') {
            proc->fd_table[i].used = 0;
            proc->fd_table[i].path[0] = '\0';
            proc->fd_table[i].can_write = 0;
            return -5;
        }
        proc->fd_table[i].path[MAX_FS_PATH_LEN - 1] = '\0';
        proc->fd_table[i].can_write = 1;
        proc->fd_table[i].offset = 0;
        return PROCESS_FD_BASE + i;
    }

    return -4;
}

// 从当前进程已打开 fd 读取数据：每次按路径重新解析文件，便于统一支持只读文件和 RAMFS 文件。
int process_read_file(int fd, char* user_buf, int size) {
    struct process* proc = current_process;
    struct process_fd_entry* entry;
    int slot;
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

    // 教学版 stdin 重定向：优先支持“从 pipe buffer 读取”，其次才是“从文件读取”。
    // pipe 读取遵循最小 EOF 语义：没有有效数据、尚未启用 pipe，或 read_offset 到达 size 时统一返回 0。
    if (fd == 0) {
        if (proc->stdin_redirect_from_pipe != 0) {
            uint32_t remaining;
            uint32_t to_copy;
            uint32_t i;

            if (process_pipe_buffer.active == 0 || process_pipe_buffer.used == 0 || process_pipe_buffer.read_offset >= process_pipe_buffer.size) {
                return 0;
            }

            remaining = process_pipe_buffer.size - process_pipe_buffer.read_offset;
            to_copy = (uint32_t)size;
            if (to_copy > remaining) {
                to_copy = remaining;
            }

            for (i = 0; i < to_copy; i++) {
                user_buf[i] = process_pipe_buffer.data[process_pipe_buffer.read_offset + i];
            }

            process_pipe_buffer.read_offset += to_copy;
            return (int)to_copy;
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

    slot = fd - PROCESS_FD_BASE;
    if (slot < 0 || slot >= PROCESS_MAX_OPEN_FILES) {
        return -3;
    }

    entry = &proc->fd_table[slot];
    if (entry->used == 0 || entry->path[0] == '\0') {
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
    int slot;
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

    slot = fd - PROCESS_FD_BASE;
    if (slot < 0 || slot >= PROCESS_MAX_OPEN_FILES) {
        return -3;
    }

    entry = &proc->fd_table[slot];
    if (entry->used == 0 || entry->path[0] == '\0') {
        return -4;
    }

    if (entry->can_write == 0) {
        return -5;
    }

    write_result = fs_write_text_file(entry->path, entry->offset, user_buf, size);
    if (write_result > 0) {
        entry->offset += (uint32_t)write_result;
    }

    return write_result;
}

// 关闭当前进程的一个教学版只读 fd：只释放表项，不涉及底层文件对象释放。
int process_close_file(int fd) {
    struct process* proc = current_process;
    int slot;

    if (proc == (struct process*)0) {
        return -1;
    }

    slot = fd - PROCESS_FD_BASE;
    if (slot < 0 || slot >= PROCESS_MAX_OPEN_FILES) {
        return -2;
    }

    if (proc->fd_table[slot].used == 0) {
        return -3;
    }

    proc->fd_table[slot].used = 0;
    proc->fd_table[slot].path[0] = '\0';
    proc->fd_table[slot].can_write = 0;
    proc->fd_table[slot].offset = 0;
    return 0;
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
