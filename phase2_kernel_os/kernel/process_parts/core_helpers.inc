// 仅在 fork 调试开启时打印无符号整数，便于验证父子 pid 和 waitpid 目标
static void process_debug_uint(unsigned int value) {
#if DEBUG_FORK
    process_print_uint(value);
#else
    (void)value;
#endif
}

// 仅在 fork 调试开启时打印 32 位十六进制地址，便于观察 eip/esp 和用户栈物理页
static void process_debug_hex(unsigned int value) {
#if DEBUG_FORK
    static const char digits[] = "0123456789ABCDEF";
    int shift;

    print_string("0x");
    for (shift = 28; shift >= 0; shift -= 4) {
        print_char(digits[(value >> shift) & 0xF]);
    }
#else
    (void)value;
#endif
}

// 统一输出最小 fork 调试日志，避免在关键路径里散落太多重复打印
static void process_debug_fork_event(const char* tag, unsigned int value1, unsigned int value2, unsigned int value3) {
#if DEBUG_FORK
    print_string("[fork] ");
    print_string(tag);
    print_string(" a=");
    process_debug_uint(value1);
    print_string(" b=");
    process_debug_uint(value2);
    print_string(" c=");
    process_debug_uint(value3);
    print_char('\n');
#else
    (void)tag;
    (void)value1;
    (void)value2;
    (void)value3;
#endif
}

// 统一输出最小 exec 调试日志，便于观察 exec 前后 pid、parent_pid 和目标程序编号
static void process_debug_exec_event(const char* tag, unsigned int value1, unsigned int value2, unsigned int value3) {
#if DEBUG_EXEC
    print_string("[exec] ");
    print_string(tag);
    print_string(" a=");
    process_debug_uint(value1);
    print_string(" b=");
    process_debug_uint(value2);
    print_string(" c=");
    process_debug_uint(value3);
    print_char('\n');
#else
    (void)tag;
    (void)value1;
    (void)value2;
    (void)value3;
#endif
}

// 裸机环境下手动打印无符号整数
static void process_print_uint(unsigned int value) {
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

// 状态码转可读字符串，供 ps 输出
const char* process_state_name(int state) {
    if (state == PROCESS_UNUSED) {
        return "UNUSED";
    }

    if (state == PROCESS_READY) {
        return "READY";
    }

    if (state == PROCESS_RUNNING) {
        return "RUNNING";
    }

    if (state == PROCESS_ZOMBIE) {
        return "ZOMBIE";
    }

    if (state == PROCESS_BLOCKED) {
        return "BLOCKED";
    }

    if (state == PROCESS_SLEEPING) {
        return "SLEEPING";
    }

    return "UNKNOWN";
}

// 当前没有真实 init 进程时，内核控制台统一用 pid 0 作为父进程
static int process_parent_pid_for_current_context(void) {
    if (current_process != (struct process*)0) {
        return current_process->pid;
    }

    return PROCESS_ROOT_PARENT_PID;
}

// 判断进程当前是否已经持有一份用户镜像，供 exec 替换语义决定是否先释放旧资源
static int process_has_user_image(struct process* proc) {
    if (proc == (struct process*)0) {
        return 0;
    }

    if (proc->user_stack_pages != 0) {
        return 1;
    }

    if (proc->user_page_count != 0) {
        return 1;
    }

    if (proc->eip != 0 || proc->esp != 0) {
        return 1;
    }

    return 0;
}

// 清空一个 PCB 槽位；本阶段只回收记录，不释放页表和用户栈等完整资源
static void process_clear_slot(struct process* proc) {
    unsigned int i;

    proc->pid = 0;
    proc->parent_pid = PROCESS_ROOT_PARENT_PID;
    proc->state = PROCESS_UNUSED;
    proc->esp = 0;
    proc->eip = 0;
    proc->exit_status = 0;
    proc->user_stack_va = 0;
    proc->user_stack_pa = 0;
    proc->user_stack_pages = 0;
    proc->user_stack_flags = 0;
    proc->user_page_count = 0;
    for (i = 0; i < ELF_LOAD_MAX_PAGES; i++) {
        proc->user_page_vaddr[i] = 0;
        proc->user_page_paddr[i] = 0;
        proc->user_page_flags[i] = 0;
    }
    proc->has_saved_frame = 0;
    proc->waiting_pid = 0;
    proc->wakeup_tick = 0;
    proc->create_tick = 0;
    proc->schedule_count = 0;
    process_clear_user_args(proc);
    process_clear_fd_table(proc);
    proc->stdout_redirect_enabled = 0;
    proc->stdout_redirect_append = 0;
    proc->stdout_redirect_started = 0;
    proc->stdout_redirect_path[0] = '\0';
    proc->stdin_redirect_enabled = 0;
    proc->stdin_redirect_path[0] = '\0';
    proc->stdin_redirect_offset = 0;
    proc->stdin_pipe_fd = -1;
    proc->stdout_pipe_fd = -1;
    proc->stdout_redirect_to_pipe = 0;
    proc->stdin_redirect_from_pipe = 0;
    proc->launch_ready = 0;
    proc->requested_exit = 0;
    proc->name = (const char*)0;
    proc->is_background = 0;
    proc->used = 0;
}

// 清空教学版单管道缓冲区：每次执行 run A | run B 前后都调用，避免残留上一次输出内容。
static void process_pipe_buffer_clear(void) {
    process_pipe_buffer.active = 0;
    process_pipe_buffer.used = 0;
    process_pipe_buffer.overflowed = 0;
    process_pipe_buffer.size = 0;
    process_pipe_buffer.read_offset = 0;
}

// 在真正决定“接下来让哪个进程运行”时记一次调度次数。
// 这里只统计被选中运行的次数，不统计 CPU 时间，也不在候选遍历阶段递增。
static void process_record_schedule(struct process* proc) {
    if (proc == (struct process*)0) {
        return;
    }

    if (proc->state == PROCESS_UNUSED || proc->state == PROCESS_ZOMBIE) {
        return;
    }

    proc->schedule_count++;
}

// 清空 PCB 中记录的教学版 fd 表；当前只重置表项，不释放静态只读文件内容。
static void process_clear_fd_table(struct process* proc) {
    unsigned int i;

    if (proc == (struct process*)0) {
        return;
    }

    for (i = 0; i < PROCESS_MAX_OPEN_FILES; i++) {
        process_fd_reset_slot(proc, (int)i);
    }
}

// 复制父进程的教学版 fd 视图：当前采用“表项浅拷贝 + 0/1 兼容字段复制”的最小策略。
// 这不是完整 POSIX 共享 file object 语义，但足以让 fork 子进程继承普通文件 fd、pipe fd 与当前 stdin/stdout 绑定关系。
static void process_copy_fd_table(struct process* child, const struct process* parent) {
    unsigned int i;

    if (child == (struct process*)0 || parent == (const struct process*)0) {
        return;
    }

    process_clear_fd_table(child);
    for (i = 0; i < PROCESS_MAX_OPEN_FILES; i++) {
        if (parent->fd_table[i].used == 0) {
            continue;
        }

        child->fd_table[i] = parent->fd_table[i];
    }

    child->stdout_redirect_enabled = parent->stdout_redirect_enabled;
    child->stdout_redirect_append = parent->stdout_redirect_append;
    child->stdout_redirect_started = parent->stdout_redirect_started;
    process_copy_path(child->stdout_redirect_path, parent->stdout_redirect_path, MAX_FS_PATH_LEN);

    child->stdin_redirect_enabled = parent->stdin_redirect_enabled;
    child->stdin_redirect_offset = parent->stdin_redirect_offset;
    process_copy_path(child->stdin_redirect_path, parent->stdin_redirect_path, MAX_FS_PATH_LEN);

    child->stdin_pipe_fd = parent->stdin_pipe_fd;
    child->stdout_pipe_fd = parent->stdout_pipe_fd;
    child->stdout_redirect_to_pipe = parent->stdout_redirect_to_pipe;
    child->stdin_redirect_from_pipe = parent->stdin_redirect_from_pipe;
}

// 把教学版 fd 编号转换成 fd_table 槽位下标；非法 fd 返回 -1。
static int process_fd_slot_from_number(int fd) {
    int slot = fd - PROCESS_FD_BASE;

    if (slot < 0 || slot >= PROCESS_MAX_OPEN_FILES) {
        return -1;
    }

    return slot;
}

// 根据 fd 编号查找当前进程的 fd 表项；非法 fd 或空进程返回空指针。
static struct process_fd_entry* process_fd_get_entry(struct process* proc, int fd) {
    int slot;

    if (proc == (struct process*)0) {
        return (struct process_fd_entry*)0;
    }

    slot = process_fd_slot_from_number(fd);
    if (slot < 0) {
        return (struct process_fd_entry*)0;
    }

    return &proc->fd_table[slot];
}

// 在当前 PCB 的 fd 表里寻找一个空闲槽位；找到返回槽位下标，失败返回 -1。
static int process_alloc_fd_slot(struct process* proc) {
    int i;

    if (proc == (struct process*)0) {
        return -1;
    }

    for (i = 0; i < PROCESS_MAX_OPEN_FILES; i++) {
        if (proc->fd_table[i].used == 0) {
            return i;
        }
    }

    return -1;
}

// 为当前教学版 pipe 分配一个 fd 端点；当前只支持 read end / write end 两种最小类型。
static int process_alloc_pipe_fd(struct process* proc, int fd_type) {
    int slot;

    if (proc == (struct process*)0) {
        return -1;
    }

    if (fd_type != PROCESS_FD_TYPE_PIPE_READ && fd_type != PROCESS_FD_TYPE_PIPE_WRITE) {
        return -2;
    }

    slot = process_alloc_fd_slot(proc);
    if (slot < 0) {
        return -3;
    }

    proc->fd_table[slot].used = 1;
    proc->fd_table[slot].type = fd_type;
    proc->fd_table[slot].path[0] = '\0';
    proc->fd_table[slot].can_write = (fd_type == PROCESS_FD_TYPE_PIPE_WRITE) ? 1 : 0;
    proc->fd_table[slot].offset = 0;
    return PROCESS_FD_BASE + slot;
}

// 重置一个 fd 表槽位：统一清空占用位、类型、路径、写标记和偏移，供 close / clear 共用。
static void process_fd_reset_slot(struct process* proc, int slot) {
    if (proc == (struct process*)0) {
        return;
    }

    if (slot < 0 || slot >= PROCESS_MAX_OPEN_FILES) {
        return;
    }

    proc->fd_table[slot].used = 0;
    proc->fd_table[slot].type = PROCESS_FD_TYPE_NONE;
    proc->fd_table[slot].path[0] = '\0';
    proc->fd_table[slot].can_write = 0;
    proc->fd_table[slot].offset = 0;
}

// 教学版内核内部 dup2 雏形：把 oldfd 对应对象复制或绑定到 newfd。
// 当前支持 newfd=0/1 的标准入口重定向，以及 newfd>=3 的 fd 表项复制；不实现引用计数或完整 POSIX 语义。
static int process_fd_dup2(struct process* proc, int oldfd, int newfd) {
    struct process_fd_entry* old_entry;
    int new_slot;

    if (proc == (struct process*)0) {
        return -1;
    }

    old_entry = process_fd_get_entry(proc, oldfd);
    if (old_entry == (struct process_fd_entry*)0 || old_entry->used == 0 || old_entry->type == PROCESS_FD_TYPE_NONE) {
        return -2;
    }

    if (newfd == oldfd) {
        return 0;
    }

    if (newfd == 0) {
        if (old_entry->type == PROCESS_FD_TYPE_PIPE_WRITE) {
            return -3;
        }

        proc->stdin_pipe_fd = -1;
        proc->stdin_redirect_from_pipe = 0;
        proc->stdin_redirect_enabled = 0;
        proc->stdin_redirect_path[0] = '\0';
        proc->stdin_redirect_offset = 0;

        if (old_entry->type == PROCESS_FD_TYPE_PIPE_READ) {
            proc->stdin_pipe_fd = oldfd;
            proc->stdin_redirect_from_pipe = 1;
            return 0;
        }

        if (process_copy_path(proc->stdin_redirect_path, old_entry->path, MAX_FS_PATH_LEN) < 0) {
            proc->stdin_redirect_path[0] = '\0';
            return -4;
        }
        proc->stdin_redirect_enabled = 1;
        proc->stdin_redirect_offset = old_entry->offset;
        return 0;
    }

    if (newfd == 1) {
        if (old_entry->type == PROCESS_FD_TYPE_PIPE_READ) {
            return -5;
        }

        proc->stdout_pipe_fd = -1;
        proc->stdout_redirect_to_pipe = 0;
        proc->stdout_redirect_enabled = 0;
        proc->stdout_redirect_append = 0;
        proc->stdout_redirect_started = 0;
        proc->stdout_redirect_path[0] = '\0';

        if (old_entry->type == PROCESS_FD_TYPE_PIPE_WRITE) {
            proc->stdout_pipe_fd = oldfd;
            proc->stdout_redirect_to_pipe = 1;
            return 0;
        }

        if (process_copy_path(proc->stdout_redirect_path, old_entry->path, MAX_FS_PATH_LEN) < 0) {
            proc->stdout_redirect_path[0] = '\0';
            return -6;
        }
        // 教学版 stdout 绑定文件时，当前仍沿用“第一次覆盖，其后追加”的重定向语义，
        // 不共享 oldfd 的实时 offset；这不是完整 POSIX dup2，只是统一入口所需的最小模型。
        proc->stdout_redirect_enabled = 1;
        proc->stdout_redirect_append = 0;
        proc->stdout_redirect_started = 0;
        return 0;
    }

    new_slot = process_fd_slot_from_number(newfd);
    if (new_slot < 0) {
        return -7;
    }

    process_fd_reset_slot(proc, new_slot);
    proc->fd_table[new_slot] = *old_entry;
    return 0;
}

// 当前进程的教学版 dup2 syscall 入口：复用内核内部 fd_dup2，并把“成功返回 newfd / 失败返回 -1”稳定暴露给用户态。
int process_dup2(int oldfd, int newfd) {
    if (current_process == (struct process*)0) {
        return -1;
    }

    if (process_fd_dup2(current_process, oldfd, newfd) < 0) {
        return -1;
    }

    return newfd;
}

// 判断一个用户态地址范围是否落在当前进程已经建立的用户页或用户栈里。
// 这里只做教学版最小检查，用来避免 pipe(fds) 这类 syscall 直接对空指针或明显越界地址写回。
static int process_user_range_is_accessible(struct process* proc, uint32_t address, uint32_t size) {
    uint32_t range_end;
    uint32_t page_end;
    uint32_t stack_end;
    uint32_t i;

    if (proc == (struct process*)0 || size == 0) {
        return 0;
    }

    if (address < 0x00400000U) {
        return 0;
    }

    range_end = address + size;
    if (range_end < address) {
        return 0;
    }

    for (i = 0; i < proc->user_page_count; i++) {
        page_end = proc->user_page_vaddr[i] + 4096U;
        if (address >= proc->user_page_vaddr[i] && range_end <= page_end) {
            return 1;
        }
    }

    if (proc->user_stack_pages != 0) {
        stack_end = proc->user_stack_va + 4096U;
        if (address >= proc->user_stack_va && range_end <= stack_end) {
            return 1;
        }
    }

    return 0;
}

// 清空 PCB 中保存的教学版 argv，避免旧程序参数泄漏到新程序上下文
static void process_clear_user_args(struct process* proc) {
    unsigned int i;
    unsigned int j;

    if (proc == (struct process*)0) {
        return;
    }

    proc->user_argc = 0;
    for (i = 0; i < PROCESS_MAX_USER_ARGS; i++) {
        for (j = 0; j < PROCESS_MAX_ARG_LEN; j++) {
            proc->user_argv[i][j] = '\0';
        }
    }
}

// 复制教学版 argv：当前保留 argv[0]=程序名 的最小语义，只支持少量短字符串，超限时返回错误码而不是继续破坏内核状态
static int process_copy_user_args(struct process* proc, int argc, const char* const* argv) {
    int i;
    int j;

    if (proc == (struct process*)0) {
        return -1;
    }

    process_clear_user_args(proc);

    if (argc < 0 || argc > PROCESS_MAX_USER_ARGS) {
        return -2;
    }

    if (argc == 0) {
        return 0;
    }

    if (argv == (const char* const*)0) {
        return -3;
    }

    for (i = 0; i < argc; i++) {
        const char* src = argv[i];

        if (src == (const char*)0) {
            process_clear_user_args(proc);
            return -4;
        }

        for (j = 0; j < PROCESS_MAX_ARG_LEN - 1; j++) {
            char ch = src[j];

            proc->user_argv[i][j] = ch;
            if (ch == '\0') {
                break;
            }
        }

        if (j == PROCESS_MAX_ARG_LEN - 1 && src[j] != '\0') {
            process_clear_user_args(proc);
            return -5;
        }

        proc->user_argv[i][PROCESS_MAX_ARG_LEN - 1] = '\0';
    }

    proc->user_argc = argc;
    return 0;
}

// 在共享页表模型里，把某个进程记录的用户页重新安装到固定用户虚拟地址
static void process_activate_user_image(struct process* proc) {
    unsigned int i;

    if (proc == (struct process*)0) {
        return;
    }

    if (proc->user_stack_pages != 0 && proc->user_stack_pa != 0 && proc->user_stack_va != 0) {
        map_page(proc->user_stack_va, proc->user_stack_pa, proc->user_stack_flags);
    }

    for (i = 0; i < proc->user_page_count && i < ELF_LOAD_MAX_PAGES; i++) {
        if (proc->user_page_vaddr[i] == 0 || proc->user_page_paddr[i] == 0) {
            continue;
        }

        map_page(proc->user_page_vaddr[i], proc->user_page_paddr[i], proc->user_page_flags[i]);
    }
}

// 裸机环境下手动复制字节，供 fork 复制用户页和保存 trapframe 时复用
static void process_copy_bytes(unsigned char* dst, const unsigned char* src, unsigned int size) {
    unsigned int i;

    for (i = 0; i < size; i++) {
        dst[i] = src[i];
    }
}

// 裸机环境下最小字符串拷贝：用于把 PCB 程序名复制到用户态可见摘要结构
static void process_copy_name(char* dst, const char* src, unsigned int max_len) {
    unsigned int i;

    if (dst == (char*)0 || max_len == 0) {
        return;
    }

    if (src == (const char*)0) {
        dst[0] = '\0';
        return;
    }

    for (i = 0; i < max_len - 1; i++) {
        dst[i] = src[i];
        if (src[i] == '\0') {
            return;
        }
    }

    dst[max_len - 1] = '\0';
}

// 裸机环境下最小路径复制：用于把 shell 传入的重定向目标复制到 PCB，避免引用用户态临时 token 缓冲区。
static int process_copy_path(char* dst, const char* src, unsigned int max_len) {
    unsigned int i;

    if (dst == (char*)0 || src == (const char*)0 || max_len == 0) {
        return -1;
    }

    for (i = 0; i < max_len - 1; i++) {
        dst[i] = src[i];
        if (src[i] == '\0') {
            return (int)i;
        }
    }

    dst[max_len - 1] = '\0';
    if (src[max_len - 1] != '\0') {
        return -2;
    }

    return (int)(max_len - 1);
}

// 统计一段以 0 结尾的文本长度；stdout 重定向沿用当前 SYS_WRITE 的字符串语义。
static int process_text_length(const char* text) {
    int length = 0;

    if (text == (const char*)0) {
        return -1;
    }

    while (text[length] != '\0') {
        length++;
    }

    return length;
}

// 最小字符串比较：用于区分当前是否处于 init 启动链路，避免在 shell wait 中走高风险阻塞切换
static int process_name_equals(const char* a, const char* b) {
    int i = 0;

    if (a == (const char*)0 || b == (const char*)0) {
        return 0;
    }

    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }

    return a[i] == '\0' && b[i] == '\0';
}

// 共享页表模型下，回收“非当前进程”会短暂解除同一组用户虚拟地址映射。
// 这里在回收后立刻把当前运行进程的用户页重装，避免 shell 后续访问触发页故障。
static void process_restore_current_user_mapping(void) {
    if (current_process == (struct process*)0) {
        return;
    }

    if (current_process->state != PROCESS_RUNNING) {
        return;
    }

    process_activate_user_image(current_process);
}

// 父进程退出时把其仍有效的子进程交给 init：只改 parent_pid，不改变子进程状态与资源
static void process_reparent_children(int old_parent_pid) {
    int i;

    if (old_parent_pid <= 0) {
        return;
    }

    // init 尚未建立时不做迁移，避免把 parent_pid 写成无效值
    if (init_pid <= 0) {
        return;
    }

    // init 自己退出时跳过 reparent，避免出现“把子进程重新挂回自己”的无效操作
    if (old_parent_pid == init_pid) {
        return;
    }

    for (i = 0; i < PROCESS_MAX; i++) {
        if (process_table[i].state == PROCESS_UNUSED) {
            continue;
        }

        if (process_table[i].parent_pid != old_parent_pid) {
            continue;
        }

        process_table[i].parent_pid = init_pid;
    }
}

// 判断某个 ZOMBIE 子进程是否正被父进程阻塞等待，避免 reaper 抢先回收 waitpid 目标。
static int process_has_blocked_waiter(struct process* child) {
    int i;

    if (child == (struct process*)0) {
        return 0;
    }

    for (i = 0; i < PROCESS_MAX; i++) {
        if (process_table[i].state != PROCESS_BLOCKED) {
            continue;
        }

        if (process_table[i].pid != child->parent_pid) {
            continue;
        }

        if (process_table[i].waiting_pid != child->pid || process_table[i].has_saved_frame == 0) {
            continue;
        }

        return 1;
    }

    return 0;
}

// init/reaper 的最小兜底：回收已经挂到 init 名下、且没有父进程正在 wait 的孤儿 ZOMBIE。
static void process_reap_init_zombies(void) {
    int i;

    if (init_pid <= 0) {
        return;
    }

    for (i = 0; i < PROCESS_MAX; i++) {
        if (process_table[i].state != PROCESS_ZOMBIE) {
            continue;
        }

        if (process_table[i].parent_pid != init_pid || process_table[i].pid == init_pid) {
            continue;
        }

        if (process_has_blocked_waiter(&process_table[i]) != 0) {
            continue;
        }

        // 这里只处理无人等待的孤儿 zombie；普通 shell 子进程仍交给 shell 的 wait/waitpid。
        process_release_user_image(&process_table[i]);
        if (last_exited_process == &process_table[i]) {
            last_exited_process = (struct process*)0;
        }
        process_clear_slot(&process_table[i]);
    }
}

// 选择下一个 READY 进程：优先从当前进程槽位后方循环扫描，实现最小轮转效果
static struct process* process_pick_next_ready(struct process* current) {
    int i;
    int start = 0;

    if (current != (struct process*)0) {
        for (i = 0; i < PROCESS_MAX; i++) {
            if (&process_table[i] == current) {
                start = i + 1;
                break;
            }
        }
    }

    for (i = 0; i < PROCESS_MAX; i++) {
        int idx = (start + i) % PROCESS_MAX;

        if (process_table[idx].state != PROCESS_READY) {
            continue;
        }

        if (process_table[idx].has_saved_frame == 0) {
            continue;
        }

        return &process_table[idx];
    }

    return (struct process*)0;
}

// 释放进程占用的最小用户镜像资源：用户栈页 + ELF 映射页
static void process_release_user_image(struct process* proc) {
    unsigned int i;

    if (proc == (struct process*)0) {
        return;
    }

    // 先释放用户栈页，再解除映射，防止 wait/waitpid 回收后残留无效映射
    if (proc->user_stack_pages != 0 && proc->user_stack_pa != 0 && proc->user_stack_va != 0) {
        unmap_page(proc->user_stack_va);
        free_page((void*)proc->user_stack_pa);
        proc->user_stack_va = 0;
        proc->user_stack_pa = 0;
        proc->user_stack_pages = 0;
    }

    // 逐页释放 ELF 代码/数据段物理页，并撤销虚拟地址映射
    for (i = 0; i < proc->user_page_count && i < ELF_LOAD_MAX_PAGES; i++) {
        if (proc->user_page_vaddr[i] != 0) {
            unmap_page(proc->user_page_vaddr[i]);
        }
        if (proc->user_page_paddr[i] != 0) {
            free_page((void*)proc->user_page_paddr[i]);
        }
        proc->user_page_vaddr[i] = 0;
        proc->user_page_paddr[i] = 0;
        proc->user_page_flags[i] = 0;
    }
    proc->user_page_count = 0;
    proc->eip = 0;
    proc->esp = 0;
}
