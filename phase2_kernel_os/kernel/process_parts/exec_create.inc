// 创建一个最小进程对象：只分配 PCB、PID 和父子关系，不负责装载 ELF
static struct process* process_create_object(void) {
    struct process* proc = process_alloc_slot();

    if (proc == (struct process*)0) {
        return (struct process*)0;
    }

    process_clear_slot(proc);
    proc->used = 1;
    proc->pid = next_pid++;
    proc->parent_pid = process_parent_pid_for_current_context();
    proc->state = PROCESS_UNUSED;
    // 新建 PCB 时记录创建 tick；后续 exec 只替换镜像，不应重置这份生命周期起点。
    proc->create_tick = pit_get_ticks();
    return proc;
}

// 把 ELF 装载结果写回 PCB，统一记录入口、栈和用户页资源范围
static void process_commit_exec_image(struct process* proc, unsigned int entry, unsigned char* stack_page, struct elf_load_info* load_info) {
    unsigned int i;

    proc->eip = entry;
    proc->esp = USER_STACK_TOP;
    proc->user_stack_va = USER_STACK_TOP - USER_STACK_SIZE;
    proc->user_stack_pa = (unsigned int)stack_page;
    proc->user_stack_pages = 1;
    proc->user_stack_flags = PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
    proc->user_page_count = load_info->page_count;

    for (i = 0; i < load_info->page_count && i < ELF_LOAD_MAX_PAGES; i++) {
        proc->user_page_vaddr[i] = load_info->page_vaddr[i];
        proc->user_page_paddr[i] = load_info->page_paddr[i];
        proc->user_page_flags[i] = load_info->page_flags[i];
    }
}

// 执行一次教学版镜像替换：先释放旧用户空间，再加载新的 ELF；失败时暂不回滚旧镜像
static int process_replace_image(struct process* proc, const unsigned char* elf_data, unsigned int elf_size) {
    process_release_user_image(proc);
    return process_exec(proc, elf_data, elf_size);
}

// 直接按统一程序描述符执行镜像替换，明确表达 program_id -> ELF/blob 的教学版语义。
static int process_exec_descriptor(struct process* proc, const struct user_program_descriptor* descriptor) {
    if (proc == (struct process*)0 || descriptor == (const struct user_program_descriptor*)0) {
        return -1;
    }

    if (descriptor->image == (const unsigned char*)0 || descriptor->image_size == 0) {
        return -2;
    }

    proc->name = descriptor->name;

    if (process_has_user_image(proc) != 0) {
        return process_replace_image(proc, descriptor->image, descriptor->image_size);
    }

    return process_exec(proc, descriptor->image, descriptor->image_size);
}

// 按 pid 查找进程，waitpid 需要先确认目标是否仍在进程表中
static struct process* process_find_by_pid(int pid) {
    int i;

    for (i = 0; i < PROCESS_MAX; i++) {
        if (process_table[i].state != PROCESS_UNUSED && process_table[i].pid == pid) {
            return &process_table[i];
        }
    }

    return (struct process*)0;
}

// 为阻塞 waitpid 查找目标子进程，既允许 READY，也允许已经成为 ZOMBIE
static struct process* process_find_child_for_parent(int pid, int parent_pid) {
    struct process* target = process_find_by_pid(pid);

    if (target == (struct process*)0) {
        return (struct process*)0;
    }

    if (target->parent_pid != parent_pid) {
        return (struct process*)0;
    }

    return target;
}

// 复制父进程已记录的最小用户镜像：逐页分配新物理页并拷贝代码/数据/用户栈内容
static int process_copy_user_image(struct process* child, struct process* parent) {
    unsigned int i;
    unsigned char* page;

    if (child == (struct process*)0 || parent == (struct process*)0) {
        return -1;
    }

    child->user_stack_va = parent->user_stack_va;
    child->user_stack_pages = parent->user_stack_pages;
    child->user_stack_flags = parent->user_stack_flags;
    child->user_page_count = parent->user_page_count;

    if (parent->user_stack_pages != 0 && parent->user_stack_pa != 0) {
        page = (unsigned char*)alloc_page();
        if (page == (unsigned char*)0) {
            process_release_user_image(child);
            return -2;
        }

        process_copy_bytes(page, (const unsigned char*)parent->user_stack_pa, USER_STACK_SIZE);
        child->user_stack_pa = (unsigned int)page;
    }

    for (i = 0; i < parent->user_page_count && i < ELF_LOAD_MAX_PAGES; i++) {
        child->user_page_vaddr[i] = parent->user_page_vaddr[i];
        child->user_page_flags[i] = parent->user_page_flags[i];

        if (parent->user_page_paddr[i] == 0) {
            continue;
        }

        page = (unsigned char*)alloc_page();
        if (page == (unsigned char*)0) {
            process_release_user_image(child);
            return -3;
        }

        process_copy_bytes(page, (const unsigned char*)parent->user_page_paddr[i], 4096);
        child->user_page_paddr[i] = (unsigned int)page;
    }

    child->eip = parent->eip;
    child->esp = parent->esp;
    return 0;
}

// 申请一个空闲 PCB 槽位
static struct process* process_alloc_slot(void) {
    int i;

    for (i = 0; i < PROCESS_MAX; i++) {
        if (process_table[i].state == PROCESS_UNUSED) {
            process_table[i].used = 1;
            return &process_table[i];
        }
    }

    return (struct process*)0;
}

// 初始化进程表：清空槽位并重置 PID 计数
void process_init(void) {
    int i;

    for (i = 0; i < PROCESS_MAX; i++) {
        process_clear_slot(&process_table[i]);
    }

    current_process = (struct process*)0;
    last_exited_process = (struct process*)0;
    process_pipe_buffer_clear();
    next_pid = 1;
    init_pid = -1;
}

// 把一份 ELF 镜像装载到指定进程中：负责用户栈、ELF 页和入口现场初始化
int process_exec(struct process* proc, const unsigned char* elf_data, unsigned int elf_size) {
    struct elf_load_info load_info;
    unsigned char* stack_page;
    unsigned int entry;

    if (proc == (struct process*)0 || elf_data == (const unsigned char*)0 || elf_size == 0) {
        return -1;
    }

    // process_exec 只负责把一份新镜像装到空进程对象；已有镜像时必须先走替换语义
    if (process_has_user_image(proc) != 0) {
        return -4;
    }

    // exec 成功后必须有一份新的用户栈，因此先准备栈页再加载 ELF
    stack_page = (unsigned char*)alloc_page();
    if (stack_page == (unsigned char*)0) {
        return -2;
    }

    map_page(USER_STACK_TOP - USER_STACK_SIZE, (unsigned int)stack_page, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    entry = elf_load_with_info(elf_data, elf_size, &load_info);
    if (entry == 0) {
        // 装载失败时只回滚本次新建的栈页；旧镜像是否还能恢复由上层语义决定
        unmap_page(USER_STACK_TOP - USER_STACK_SIZE);
        free_page((void*)stack_page);
        return -3;
    }

    process_commit_exec_image(proc, entry, stack_page, &load_info);
    proc->exit_status = 0;
    proc->has_saved_frame = 0;
    return 0;
}

// 按文件名装载或替换指定进程的用户镜像，供 process_create 与未来 syscall exec 复用
int process_exec_file(struct process* proc, const char* name) {
    struct file* target;

    if (proc == (struct process*)0 || name == (const char*)0 || name[0] == '\0') {
        return -1;
    }

    target = fs_find(name);
    if (target == (struct file*)0) {
        return -2;
    }

    // 先更新进程名：即使后续装载失败，也能在 ps 中看到“本次尝试 exec 的目标程序名”，便于排查启动路径问题。
    proc->name = target->name;

    if (process_has_user_image(proc) != 0) {
        if (process_replace_image(proc, (const unsigned char*)target->data, (unsigned int)target->size) != 0) {
            return -3;
        }
    } else {
        if (process_exec(proc, (const unsigned char*)target->data, (unsigned int)target->size) != 0) {
            return -3;
        }
    }

    return 0;
}

// 当前运行进程执行教学版最小 exec：仅支持固定 program_id，对应内置用户程序
int process_exec_program(int program_id, struct interrupt_frame* frame) {
    return process_exec_program_args(program_id, 0, (const char* const*)0, frame);
}

// 当前运行进程执行教学版最小 exec：参数先拷贝到 PCB，再把当前用户镜像替换成目标程序
int process_exec_program_args(int program_id, int argc, const char* const* argv, struct interrupt_frame* frame) {
    struct process* proc = current_process;
    const struct user_program_descriptor* target_program;
    int arg_result;
    int exec_result;

    if (proc == (struct process*)0 || frame == (struct interrupt_frame*)0) {
        return -1;
    }

    target_program = user_program_get_by_id(program_id);
    if (target_program == (const struct user_program_descriptor*)0) {
        return -2;
    }

    arg_result = process_copy_user_args(proc, argc, argv);
    if (arg_result != 0) {
        // 参数复制失败时把底层错误码继续向上传递，便于 shell 和文档统一描述边界语义。
        return arg_result;
    }

    process_debug_exec_event("begin", (unsigned int)proc->pid, (unsigned int)proc->parent_pid, (unsigned int)program_id);
    exec_result = process_exec_descriptor(proc, target_program);
    if (exec_result != 0) {
        print_string("process: exec failed target=");
        print_string(target_program->name);
        print_string("\n");
        process_clear_user_args(proc);
        process_debug_exec_event("failed", (unsigned int)proc->pid, (unsigned int)proc->parent_pid, (unsigned int)program_id);
        return -3;
    }

    process_activate_user_image(proc);

    // exec 成功后直接改写本次 syscall 返回现场，确保不会回到旧程序的 fork 后续逻辑
    frame->eax = 0;
    frame->ebx = 0;
    frame->ecx = 0;
    frame->edx = 0;
    frame->esi = 0;
    frame->edi = 0;
    frame->ebp = 0;
    frame->eip = proc->eip;
    frame->user_esp = proc->esp;

    process_debug_exec_event("replace", (unsigned int)proc->pid, (unsigned int)proc->parent_pid, (unsigned int)program_id);
    return 0;
}

// 返回当前进程保存的教学版 argc，供用户程序在最小 syscall ABI 下读取启动参数数量
int process_get_argc(void) {
    if (current_process == (struct process*)0) {
        return -1;
    }

    return current_process->user_argc;
}

// 把当前进程保存的一项教学版 argv 复制到用户缓冲区；当前阶段依赖共享映射，指针校验仍很有限
int process_get_arg(int index, char* user_buf, int max_len) {
    int i;
    struct process* proc = current_process;

    if (proc == (struct process*)0 || user_buf == (char*)0) {
        return -1;
    }

    if (index < 0 || index >= proc->user_argc) {
        return -2;
    }

    if (max_len <= 0 || max_len > PROCESS_MAX_ARG_LEN) {
        return -3;
    }

    for (i = 0; i < max_len - 1; i++) {
        char ch = proc->user_argv[index][i];

        user_buf[i] = ch;
        if (ch == '\0') {
            return i;
        }
    }

    user_buf[max_len - 1] = '\0';
    if (proc->user_argv[index][max_len - 1] != '\0') {
        return -4;
    }

    return max_len - 1;
}

// 按文件名创建进程：先创建 PCB，再调用 exec 语义装载用户镜像
struct process* process_create(const char* name) {
    struct process* proc;
    int exec_result;

    if (name == (const char*)0 || name[0] == '\0') {
        print_string("process: empty name\n");
        return (struct process*)0;
    }

    proc = process_create_object();
    if (proc == (struct process*)0) {
        print_string("process: table full\n");
        return (struct process*)0;
    }

    exec_result = process_exec_file(proc, name);
    if (exec_result != 0) {
        if (exec_result == -2) {
            print_string("process: file not found\n");
        } else {
            print_string("process: exec load failed\n");
        }
        process_release_user_image(proc);
        process_clear_slot(proc);
        return (struct process*)0;
    }
    proc->state = PROCESS_READY;
    // 记录第一个 init 进程 pid，作为教学版孤儿进程 reparent 的目标父进程。
    if (init_pid <= 0 && process_name_equals(name, "init") != 0) {
        init_pid = proc->pid;
    }
    return proc;
}
