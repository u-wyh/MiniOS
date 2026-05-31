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
