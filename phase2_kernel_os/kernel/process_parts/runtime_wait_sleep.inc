// 运行指定进程：更新状态并进入用户态执行
void process_run(struct process* proc) {
    if (proc == (struct process*)0 || proc->state != PROCESS_READY) {
        return;
    }

    if (current_process != (struct process*)0 && current_process->state == PROCESS_RUNNING) {
        current_process->state = PROCESS_READY;
    }

    current_process = proc;
    current_process->state = PROCESS_RUNNING;
    // 首次进入用户态前，也算调度器把该进程选中运行了一次。
    process_record_schedule(current_process);
    process_activate_user_image(current_process);

    enter_user_mode(current_process->eip, current_process->esp);
}

// 将当前进程标记为 ZOMBIE，并保留 exit_status 供父进程 wait/reaper 读取和回收。
void process_exit(int status) {
    int exiting_pid;

    if (current_process == (struct process*)0) {
        return;
    }

    exiting_pid = current_process->pid;
    // 进程退出前先按当前教学版 close 语义清理自己的 fd 视图。
    // 这里只关闭当前进程持有的端点，不主动丢弃全局 pipe buffer 里已经写入的数据。
    process_close_all_fds_for_proc(current_process);
    // 当前进程退出前先处理孤儿进程：把它的子进程交给 init，避免子进程失去可回收父进程。
    process_reparent_children(exiting_pid);

    current_process->exit_status = status;
    current_process->state = PROCESS_ZOMBIE;
    last_exited_process = current_process;
    current_process = (struct process*)0;
    // 如果退出的是已经 reparent 给 init 的孤儿进程，init/reaper 可直接完成兜底回收。
    process_reap_init_zombies();
}

// 回收一个 ZOMBIE 子进程；当前最小 wait 语义是“只回收已经退出的子进程”，不做复杂阻塞等待。
int process_wait(void) {
    int i;
    int pid;
    int parent_pid = process_parent_pid_for_current_context();

    for (i = 0; i < PROCESS_MAX; i++) {
        if (process_table[i].state != PROCESS_ZOMBIE) {
            continue;
        }

        if (process_table[i].parent_pid != parent_pid) {
            continue;
        }

        pid = process_table[i].pid;
            // wait 时先释放子进程资源，再回收 PCB；exit_status 已经在 ZOMBIE 期间对父进程可见。
            process_release_user_image(&process_table[i]);
        process_restore_current_user_mapping();
        if (last_exited_process == &process_table[i]) {
            last_exited_process = (struct process*)0;
        }
        process_clear_slot(&process_table[i]);
        return pid;
    }

    return -1;
}

// waitpid 雏形：目标已成为 ZOMBIE 时立即回收；否则由 syscall 路径决定是否进入最小阻塞等待。
int process_waitpid(int pid) {
    struct process* target = process_find_by_pid(pid);
    int parent_pid = process_parent_pid_for_current_context();

    if (target == (struct process*)0) {
        return -1;
    }

    if (target->parent_pid != parent_pid) {
        return -2;
    }

    if (target->state != PROCESS_ZOMBIE) {
        return -3;
    }

    // waitpid 回收前释放用户页资源，确保 PCB 复用时不会累计泄漏
    process_release_user_image(target);
    process_restore_current_user_mapping();
    if (last_exited_process == target) {
        last_exited_process = (struct process*)0;
    }
    process_clear_slot(target);
    return pid;
}

// 非阻塞回收任意 ZOMBIE 子进程：仅扫描当前进程名下的子进程，没有可回收目标时返回 0
int process_wait_any(void) {
    int i;
    int parent_pid;
    int reaped_pid;

    if (current_process == (struct process*)0) {
        return -1;
    }

    parent_pid = current_process->pid;
    for (i = 0; i < PROCESS_MAX; i++) {
        if (process_table[i].state != PROCESS_ZOMBIE) {
            continue;
        }

        if (process_table[i].parent_pid != parent_pid) {
            continue;
        }

        // 复用既有回收路径：先释放用户镜像资源，再回收 PCB 槽位
        process_release_user_image(&process_table[i]);
        process_restore_current_user_mapping();
        if (last_exited_process == &process_table[i]) {
            last_exited_process = (struct process*)0;
        }
        reaped_pid = process_table[i].pid;
        process_clear_slot(&process_table[i]);
        return reaped_pid;
    }

    return 0;
}

// 用户态 yield：保存当前进程现场并切换到下一个 READY 进程；没有可切换目标时直接返回
int process_yield_syscall(struct interrupt_frame* frame, struct interrupt_frame** next_frame) {
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
    current->state = PROCESS_READY;

    next = process_pick_next_ready(current);
    if (next == (struct process*)0 || next == current) {
        current->state = PROCESS_RUNNING;
        // 当前进程主动 yield 但没有其他候选时，继续运行当前进程也记作一次最小调度决策。
        process_record_schedule(current);
        return 0;
    }

    current_process = next;
    next->state = PROCESS_RUNNING;
    // 只有真正切到 next 运行时，才给 next 增加调度次数。
    process_record_schedule(next);
    process_activate_user_image(next);
    if (next_frame != (struct interrupt_frame**)0) {
        *next_frame = &next->saved_frame;
    }
    return -4;
}

// 用户态 sleep：把当前进程标记为 SLEEPING 并设置唤醒 tick，然后切换到其他 READY 进程
int process_sleep_syscall(unsigned int ticks, struct interrupt_frame* frame, struct interrupt_frame** next_frame) {
    struct process* current = current_process;
    struct process* next;
    unsigned int now;

    if (ticks == 0) {
        return process_yield_syscall(frame, next_frame);
    }

    if (next_frame != (struct interrupt_frame**)0) {
        *next_frame = (struct interrupt_frame*)0;
    }

    if (current == (struct process*)0 || frame == (struct interrupt_frame*)0) {
        return -1;
    }

    now = pit_get_ticks();
    current->saved_frame = *frame;
    current->has_saved_frame = 1;
    current->wakeup_tick = now + ticks;
    current->state = PROCESS_SLEEPING;

    next = process_pick_next_ready(current);
    if (next == (struct process*)0 || next == current) {
        // 没有可切换目标时，不再回滚成失败，而是让当前进程保持 SLEEPING，
        // 由内核 idle 路径 hlt 等待 PIT tick，到期后再恢复这个进程。
        current_process = (struct process*)0;
        return -5;
    }

    current_process = next;
    next->state = PROCESS_RUNNING;
    // sleep 让出 CPU 后，只有被选中的 READY 进程才增加调度次数。
    process_record_schedule(next);
    process_activate_user_image(next);
    if (next_frame != (struct interrupt_frame**)0) {
        *next_frame = &next->saved_frame;
    }
    return -4;
}

// PIT 每次 tick 调用：把到期的 SLEEPING 进程改回 READY，等待调度器后续选中运行
void process_wakeup_sleeping(unsigned int now_tick) {
    int i;

    for (i = 0; i < PROCESS_MAX; i++) {
        if (process_table[i].state != PROCESS_SLEEPING) {
            continue;
        }

        if (now_tick < process_table[i].wakeup_tick) {
            continue;
        }

        process_table[i].wakeup_tick = 0;
        process_table[i].state = PROCESS_READY;
    }
}

// 按 pid 设置睡眠：教学版调试接口，仅修改目标状态与唤醒 tick，不做复杂抢占
int process_sleep_pid(int pid, unsigned int ticks) {
    struct process* target;

    if (ticks == 0) {
        return 0;
    }

    target = process_find_by_pid(pid);
    if (target == (struct process*)0) {
        return -1;
    }

    if (target->state == PROCESS_UNUSED || target->state == PROCESS_ZOMBIE) {
        return -2;
    }

    // 避免把当前执行进程直接改为 SLEEPING 却不切换现场，当前最小实现先拒绝该情况
    if (target == current_process) {
        return -3;
    }

    target->wakeup_tick = pit_get_ticks() + ticks;
    target->state = PROCESS_SLEEPING;
    return 0;
}
