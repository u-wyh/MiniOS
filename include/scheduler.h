#ifndef MINIOS_SCHEDULER_H
#define MINIOS_SCHEDULER_H

#include <deque>
#include <string>
#include <vector>

enum class SchedulerPolicy {
    RoundRobin
};

class Scheduler {
public:
    // 默认初始化为 RR 策略，currentTid = -1 表示当前未选择任务。
    Scheduler();

    // 新任务进入可运行集合时加入 ready queue。
    void addTask(int tid);
    // 任务结束/被杀时从 ready queue 和 current 中移除。
    void removeTask(int tid);
    // 手动推进一次调度周期（本轮为 RR 轮转）。
    void tick();
    // 输出策略、当前任务和就绪队列快照。
    void status() const;
    // 设置调度策略（当前仅支持 rr）。
    void setPolicy(const std::string& policy);

private:
    // 当前调度策略。
    SchedulerPolicy policy;
    // 就绪队列：以 tid 为调度单位。
    std::deque<int> readyQueue;
    // 当前被选中运行的 tid；-1 表示无 current。
    int currentTid;
};

// 获取全局调度器实例，供 TaskManager 共享使用。
Scheduler& getScheduler();

// 处理 sched 命令：status / tick / policy rr。
bool executeSchedulerCommand(const std::vector<std::string>& tokens);

#endif
