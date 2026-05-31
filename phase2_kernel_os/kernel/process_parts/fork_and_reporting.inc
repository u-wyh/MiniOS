// 基于当前运行进程的 trapframe 构造一个教学版 fork 子进程
int process_fork(struct interrupt_frame* frame) {
    struct process* parent = current_process;
    struct process* child;
    int copy_result;

    if (parent == (struct process*)0 || frame == (struct interrupt_frame*)0) {
        return -1;
    }

    child = process_create_object();
    if (child == (struct process*)0) {
        return -2;
    }

    child->parent_pid = parent->pid;
    child->name = parent->name;
    child->is_background = 0;
    child->stdout_redirect_enabled = 0;
    child->stdout_redirect_append = 0;
    child->stdout_redirect_started = 0;
    child->stdout_redirect_path[0] = '\0';
    child->stdin_redirect_enabled = 0;
    child->stdin_redirect_path[0] = '\0';
    child->stdin_redirect_offset = 0;
    child->stdout_redirect_to_pipe = 0;
    child->stdin_redirect_from_pipe = 0;
    child->launch_ready = 0;
    child->exit_status = 0;
    child->requested_exit = 0;
    // fork 会创建一个新的进程实体，所以子进程需要拥有新的 create_tick。
    child->create_tick = pit_get_ticks();
    child->user_argc = parent->user_argc;
    process_copy_bytes((unsigned char*)child->user_argv, (const unsigned char*)parent->user_argv, sizeof(child->user_argv));
    // 当前教学版 fd 层先不实现 fork 继承，避免把半成品共享/引用计数语义带进来。
    process_clear_fd_table(child);

    copy_result = process_copy_user_image(child, parent);
    if (copy_result != 0) {
        process_release_user_image(child);
        process_clear_slot(child);
        return -3;
    }

    // 子进程从和父进程相同的用户态返回点继续执行，但 fork 返回值必须是 0
    child->saved_frame = *frame;
    child->saved_frame.eax = 0;
    child->has_saved_frame = 1;
    // fork 的默认语义仍保持 READY；只有 shell 的 run/start 子分支会主动等待 launch_ready 门闩。
    child->state = PROCESS_READY;

    // fork 调试输出：验证父子 pid、parent_pid、返回点 eip 和用户栈物理页都符合预期
    process_debug_fork_event("clone", (unsigned int)parent->pid, (unsigned int)child->pid, (unsigned int)child->parent_pid);
#if DEBUG_FORK
    print_string("[fork] return parent=");
    process_debug_uint((unsigned int)child->pid);
    print_string(" child=0 eip=");
    process_debug_hex(child->saved_frame.eip);
    print_string(" esp=");
    process_debug_hex(child->saved_frame.user_esp);
    print_char('\n');
    print_string("[fork] stack parent_pa=");
    process_debug_hex(parent->user_stack_pa);
    print_string(" child_pa=");
    process_debug_hex(child->user_stack_pa);
    print_char('\n');
#endif
    return child->pid;
}

// 用户态 waitpid：目标未退出时阻塞父进程，并切换到子进程继续执行
int process_waitpid_syscall(int pid, struct interrupt_frame* frame, struct interrupt_frame** next_frame) {
    struct process* parent = current_process;
    struct process* child;
    int result;

    if (next_frame != (struct interrupt_frame**)0) {
        *next_frame = (struct interrupt_frame*)0;
    }

    if (parent == (struct process*)0 || frame == (struct interrupt_frame*)0) {
        return -1;
    }

    result = process_waitpid(pid);
    if (result >= 0) {
        process_debug_fork_event("waitpid fast reap", (unsigned int)parent->pid, (unsigned int)pid, (unsigned int)result);
        return result;
    }

    if (result != -3) {
        return result;
    }

    child = process_find_child_for_parent(pid, parent->pid);
    if (child == (struct process*)0 || child->state != PROCESS_READY || child->has_saved_frame == 0) {
        return -3;
    }

    // waitpid 在教学版 fork 中采用最小阻塞语义：父进程保存现场后，把 CPU 让给子进程。
    // 这里同时服务于 init 等待 shell、shell 前台 run，以及 shell 手动 wait 后台子进程。
    parent->saved_frame = *frame;
    parent->has_saved_frame = 1;
    parent->waiting_pid = pid;
    parent->state = PROCESS_BLOCKED;

    process_debug_fork_event("waitpid block", (unsigned int)parent->pid, (unsigned int)pid, (unsigned int)child->pid);

    current_process = child;
    child->state = PROCESS_RUNNING;
    // 父进程阻塞等待时，真正拿到 CPU 继续运行的是目标子进程。
    process_record_schedule(child);
    process_activate_user_image(child);
    if (next_frame != (struct interrupt_frame**)0) {
        *next_frame = &child->saved_frame;
    }

    return -4;
}

// 教学版 kill：按 pid 把目标普通用户进程标记为 ZOMBIE，资源释放仍交给父进程 wait/waitpid 回收。
// exit_code 通常使用 PROCESS_KILL_EXIT_STATUS，仅表示“被 kill”，不是完整信号编号。
int process_kill(int pid, int exit_code) {
    struct process* target = process_find_by_pid(pid);

    if (target == (struct process*)0) {
        return -1;
    }

    // 当前阶段最小保护：不允许杀 init(约定 pid=1)，避免系统失去根父进程
    if (target->pid == 1) {
        return -2;
    }

    // 不允许直接杀当前正在执行的进程，避免在本轮引入自杀路径的复杂恢复逻辑
    if (current_process != (struct process*)0 && target->pid == current_process->pid) {
        return -3;
    }

    if (target->state == PROCESS_UNUSED) {
        return -4;
    }

    if (target->state == PROCESS_ZOMBIE) {
        return -5;
    }

    // kill 让目标直接进入退出路径；若目标还有子进程，也要先转交给 init。
    process_reparent_children(target->pid);
    target->exit_status = exit_code;
    target->state = PROCESS_ZOMBIE;
    target->has_saved_frame = 0;
    target->waiting_pid = 0;
    last_exited_process = target;
    // 被 kill 的进程如果已经是 init 名下孤儿，交给最小 reaper 兜底清理。
    process_reap_init_zombies();
    return 0;
}

// 子进程 exit 后，若父进程正阻塞等待它，则直接回收子进程并恢复父进程
struct interrupt_frame* process_resume_after_exit(void) {
    struct process* child;
    struct process* parent;
    int child_pid;
    int i;

    child = last_exited_process;
    child_pid = 0;

    if (child == (struct process*)0 || child->state != PROCESS_ZOMBIE || child->pid == 0) {
        return (struct interrupt_frame*)0;
    }

    child_pid = child->pid;

    for (i = 0; i < PROCESS_MAX; i++) {
        if (process_table[i].state != PROCESS_BLOCKED) {
            continue;
        }

        if (process_table[i].pid != child->parent_pid) {
            continue;
        }

        if (process_table[i].waiting_pid != child_pid || process_table[i].has_saved_frame == 0) {
            continue;
        }

        parent = &process_table[i];

        // init 当前只会对“用户明确执行了 exit 的 shell”做自动重启。
        // 若 shell 在未显式请求退出时就结束，这里只做资源回收，不唤醒 init，避免系统悄悄拉起一个新 shell。
        if (process_is_init_waiting_shell_restart(child, parent) != 0) {
            process_release_user_image(child);
            process_clear_slot(child);
            last_exited_process = (struct process*)0;
            return (struct interrupt_frame*)0;
        }

        parent->saved_frame.eax = (unsigned int)child_pid;
        parent->waiting_pid = 0;

        process_debug_fork_event("resume parent", (unsigned int)parent->pid, (unsigned int)child_pid, (unsigned int)child->exit_status);

        process_release_user_image(child);
        process_clear_slot(child);
        last_exited_process = (struct process*)0;

        current_process = parent;
        parent->state = PROCESS_RUNNING;
        // 子进程退出后恢复父进程执行，也记作父进程再次被调度运行。
        process_record_schedule(parent);
        process_activate_user_image(parent);
        return &parent->saved_frame;
    }

    return (struct interrupt_frame*)0;
}

// 判断这次退出是否属于“init 正在等待 shell，但 shell 并非由用户主动 exit”的情况。
// 命中后不恢复 init，从而避免异常退出导致 shell 被静默自动重启。
static int process_is_init_waiting_shell_restart(struct process* child, struct process* parent) {
    if (child == (struct process*)0 || parent == (struct process*)0) {
        return 0;
    }

    if (init_pid <= 0 || parent->pid != init_pid) {
        return 0;
    }

    if (child->name == (const char*)0 || process_name_equals(child->name, "shell") == 0) {
        return 0;
    }

    if (child->requested_exit != 0) {
        return 0;
    }

    return 1;
}

// 输出进程列表：PID、PPID、STATE、退出码与程序名
void process_list(void) {
    int i;
    unsigned int now = pit_get_ticks();

    print_string("PID   PPID   STATE    AGE   RUNS   EXIT    NAME\n");
    for (i = 0; i < PROCESS_MAX; i++) {
        unsigned int age_ticks;

        if (process_table[i].state == PROCESS_UNUSED) {
            continue;
        }

        if (process_table[i].create_tick == 0) {
            age_ticks = 0;
        } else {
            age_ticks = now - process_table[i].create_tick;
        }

        process_print_uint((unsigned int)process_table[i].pid);
        print_string("    ");
        process_print_uint((unsigned int)process_table[i].parent_pid);
        print_string("      ");
        print_string(process_state_name(process_table[i].state));
        print_string("    ");
        process_print_uint(age_ticks);
        print_string("    ");
        process_print_uint(process_table[i].schedule_count);
        print_string("    ");
        process_print_uint((unsigned int)process_table[i].exit_status);
        print_string("       ");
        if (process_table[i].name != (const char*)0) {
            print_string(process_table[i].name);
        } else {
            print_string("(none)");
        }
        print_char('\n');
    }
}

// 按活动进程序号导出只读摘要：用户态 ps 通过 syscall 逐条读取，避免直接暴露 PCB 指针
int process_get_info_by_index(int index, struct process_info* out) {
    int i;
    int current = 0;
    unsigned int now;

    if (index < 0 || out == (struct process_info*)0) {
        return -1;
    }

    now = pit_get_ticks();
    for (i = 0; i < PROCESS_MAX; i++) {
        if (process_table[i].state == PROCESS_UNUSED) {
            continue;
        }

        if (current == index) {
            out->pid = process_table[i].pid;
            out->ppid = process_table[i].parent_pid;
            out->state = process_table[i].state;
            // age_ticks 只表示“从创建到现在过去了多少 tick”，不是 CPU 实际运行时间。
            if (process_table[i].create_tick == 0) {
                out->age_ticks = 0;
            } else {
            out->age_ticks = now - process_table[i].create_tick;
            }
            out->runs = process_table[i].schedule_count;
            out->exit_status = process_table[i].exit_status;
            out->is_background = process_table[i].is_background;
            process_copy_name(out->name, process_table[i].name, PROCESS_NAME_MAX_LEN);
            return 0;
        }

        current++;
    }

    return -1;
}

// 返回已创建的进程数量
int process_count(void) {
    int i;
    int count = 0;

    for (i = 0; i < PROCESS_MAX; i++) {
        if (process_table[i].state != PROCESS_UNUSED) {
            count++;
        }
    }

    return count;
}

// PIT 时间片切换入口：把当前 RUNNING 进程的中断现场保存到 saved_frame，
// 再按最小轮转规则选择下一个 READY 进程；若没有合适目标则保持当前进程继续运行。
unsigned int process_schedule_tick(unsigned int current_esp) {
    struct process* current = current_process;
    struct process* next;
    struct interrupt_frame* frame = (struct interrupt_frame*)current_esp;

    // 当 CPU 处于内核 idle/hlt 路径时，current_process 可能为空。
    // 这时只要有 READY 的用户进程，就直接切到它，避免“睡眠后只能忙等”。
    if (current == (struct process*)0) {
        next = process_pick_next_ready((struct process*)0);
        if (next == (struct process*)0) {
            return current_esp;
        }

        current_process = next;
        next->state = PROCESS_RUNNING;
        // idle 路径重新选中某个 READY 进程时，也需要记录一次调度次数。
        process_record_schedule(next);
        process_activate_user_image(next);
        return (unsigned int)&next->saved_frame;
    }

    if (frame == (struct interrupt_frame*)0) {
        return current_esp;
    }

    if (current->state != PROCESS_RUNNING) {
        return current_esp;
    }

    current->saved_frame = *frame;
    current->has_saved_frame = 1;
    current->state = PROCESS_READY;

    next = process_pick_next_ready(current);
    if (next == (struct process*)0 || next == current) {
        current->state = PROCESS_RUNNING;
        // 时间片到期后即使继续运行当前进程，也按一次最小调度决策记账。
        process_record_schedule(current);
        process_activate_user_image(current);
        return (unsigned int)&current->saved_frame;
    }

    current_process = next;
    next->state = PROCESS_RUNNING;
    // 只有在确定 next 即将运行时，才给它增加调度次数。
    process_record_schedule(next);
    process_activate_user_image(next);
    return (unsigned int)&next->saved_frame;
}
