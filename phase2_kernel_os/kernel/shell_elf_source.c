// shell_elf_source.c：用户态最小 shell 源文件，用于重新生成嵌入式 shell_elf.inc

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_FORK 5
#define SYS_WAITPID 6
#define SYS_READ_CHAR 8
#define SYS_EXEC_ARGS 11
#define SYS_PS 12
#define SYS_KILL 13
#define SYS_SLEEP 16
#define SYS_SLEEP_PID 17
#define SYS_SET_BACKGROUND 18
#define SYS_GET_TICKS 19

#define PROCESS_NAME_MAX_LEN 16
#define SHELL_ARGV_MAX 8
#define SHELL_LINE_MAX 128

struct process_info {
    int pid;
    int ppid;
    int state;
    unsigned int age_ticks;
    char name[PROCESS_NAME_MAX_LEN];
};

// 最小 int 0x80 包装：保持和当前教学版用户程序 ABI 一致。
static int user_syscall0(int number) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(number) : "memory");
    return result;
}

// 一个参数的系统调用封装。
static int user_syscall1(int number, int arg1) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(number), "b"(arg1) : "memory");
    return result;
}

// 两个参数的系统调用封装。
static int user_syscall2(int number, int arg1, int arg2) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(number), "b"(arg1), "c"(arg2) : "memory");
    return result;
}

// 三个参数的系统调用封装。
static int user_syscall3(int number, int arg1, int arg2, int arg3) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3) : "memory");
    return result;
}

// 向控制台输出字符串。
static void user_write(const char* text) {
    user_syscall1(SYS_WRITE, (int)text);
}

// 退出当前用户进程。
static void user_exit(int status) {
    user_syscall1(SYS_EXIT, status);
    for (;;) {
    }
}

// 读取一个字符；无输入时内核会阻塞当前 shell。
static int user_read_char(void) {
    return user_syscall0(SYS_READ_CHAR);
}

// 让当前进程睡眠若干 tick。
static int user_sleep_ticks(unsigned int ticks) {
    return user_syscall1(SYS_SLEEP, (int)ticks);
}

// 教学调试接口：按 pid 让目标进程睡眠。
static int user_sleep_pid(int pid, unsigned int ticks) {
    return user_syscall2(SYS_SLEEP_PID, pid, (int)ticks);
}

// 读取系统启动以来的 tick 数。
static unsigned int user_get_ticks(void) {
    return (unsigned int)user_syscall0(SYS_GET_TICKS);
}

// 创建子进程。
static int user_fork(void) {
    return user_syscall0(SYS_FORK);
}

// 等待指定子进程退出并回收。
static int user_waitpid(int pid) {
    return user_syscall1(SYS_WAITPID, pid);
}

// 以带参数方式执行教学版最小 exec。
static int user_exec_args(int program_id, int argc, const char* const* argv) {
    return user_syscall3(SYS_EXEC_ARGS, program_id, argc, (int)argv);
}

// 逐条读取 ps 摘要。
static int user_ps_get(int index, struct process_info* info) {
    return user_syscall2(SYS_PS, index, (int)info);
}

// 终止指定 pid。
static int user_kill(int pid) {
    return user_syscall1(SYS_KILL, pid);
}

// 设置后台标记，供 start 命令使用。
static int user_set_background(int pid, int is_background) {
    return user_syscall2(SYS_SET_BACKGROUND, pid, is_background);
}

// 比较两个字符串是否相等。
static int shell_streq(const char* left, const char* right) {
    int i = 0;

    if (left == (const char*)0 || right == (const char*)0) {
        return 0;
    }

    while (left[i] != '\0' && right[i] != '\0') {
        if (left[i] != right[i]) {
            return 0;
        }
        i++;
    }

    return left[i] == '\0' && right[i] == '\0';
}

// 输出十进制整数，便于显示 pid/tick/age。
static void shell_write_uint(int value) {
    char digits[16];
    int count = 0;
    unsigned int current;

    if (value == 0) {
        user_write("0");
        return;
    }

    if (value < 0) {
        user_write("-");
        current = (unsigned int)(-value);
    } else {
        current = (unsigned int)value;
    }

    while (current != 0) {
        digits[count++] = (char)('0' + (current % 10));
        current /= 10;
    }

    while (count > 0) {
        char one[2];
        count--;
        one[0] = digits[count];
        one[1] = '\0';
        user_write(one);
    }
}

// 把十进制字符串解析成整数；失败返回 0。
static int shell_atoi(const char* text, int* out_value) {
    int value = 0;
    int i = 0;

    if (text == (const char*)0 || text[0] == '\0' || out_value == (int*)0) {
        return 0;
    }

    while (text[i] != '\0') {
        if (text[i] < '0' || text[i] > '9') {
            return 0;
        }
        value = value * 10 + (text[i] - '0');
        i++;
    }

    *out_value = value;
    return 1;
}

// 逐字符读入一行，并在本地回显。
static int shell_read_line(char* buffer, int capacity) {
    int length = 0;

    for (;;) {
        int ch = user_read_char();

        if (ch == '\r' || ch == '\n') {
            user_write("\n");
            buffer[length] = '\0';
            return length;
        }

        if (ch == '\b') {
            if (length > 0) {
                length--;
                user_write("\b \b");
            }
            continue;
        }

        if (ch <= 31 || ch > 126) {
            continue;
        }

        if ((length + 1) >= capacity) {
            continue;
        }

        buffer[length++] = (char)ch;
        {
            char one[2];
            one[0] = (char)ch;
            one[1] = '\0';
            user_write(one);
        }
    }
}

// 按空格或 tab 拆分参数，直接在原始缓冲区上写入 '\0'。
static int shell_split_line(char* line, char** argv, int max_argv) {
    int argc = 0;
    char* current = line;

    while (*current != '\0') {
        while (*current == ' ' || *current == '\t') {
            current++;
        }

        if (*current == '\0') {
            break;
        }

        if (argc >= max_argv) {
            break;
        }

        argv[argc++] = current;
        while (*current != '\0' && *current != ' ' && *current != '\t') {
            current++;
        }

        if (*current == '\0') {
            break;
        }

        *current = '\0';
        current++;
    }

    return argc;
}

// 把程序名映射到当前内置 program_id。
static int shell_program_id_from_name(const char* name) {
    if (shell_streq(name, "hello")) {
        return 3;
    }

    if (shell_streq(name, "echo")) {
        return 4;
    }

    if (shell_streq(name, "loop")) {
        return 5;
    }

    if (shell_streq(name, "sleep_test")) {
        return 6;
    }

    // 兼容不便输入下划线的场景，允许用 sleeptest 作为 sleep_test 的简写别名。
    if (shell_streq(name, "sleeptest")) {
        return 6;
    }

    return -1;
}

// 统一处理 run/start/hello 的 fork + exec + waitpid 逻辑。
static int shell_spawn_program(int program_id, int argc, char** argv, int wait_child, int is_background) {
    int pid = user_fork();

    if (pid < 0) {
        user_write("Fork failed\n");
        return -1;
    }

    if (pid == 0) {
        if (user_exec_args(program_id, argc, (const char* const*)argv) != 0) {
            user_exit(99);
        }
        user_exit(0);
    }

    if (is_background != 0) {
        user_set_background(pid, 1);
    }

    if (wait_child != 0) {
        if (user_waitpid(pid) < 0) {
            user_write("Wait failed\n");
            return -1;
        }
        return 0;
    }

    user_write("Started pid: ");
    shell_write_uint(pid);
    user_write("\n");
    return pid;
}

// 输出 echo 的参数。
static void shell_cmd_echo(int argc, char** argv) {
    int i;

    if (argc <= 1) {
        user_write("\n");
        return;
    }

    for (i = 1; i < argc; i++) {
        user_write(argv[i]);
        if ((i + 1) < argc) {
            user_write(" ");
        }
    }
    user_write("\n");
}

// 输出当前 tick。
static void shell_cmd_uptime(void) {
    user_write("ticks: ");
    shell_write_uint((int)user_get_ticks());
    user_write("\n");
}

// 输出 ps 结果，并显示 AGE 列。
static void shell_cmd_ps(void) {
    int index = 0;
    struct process_info info;

    user_write("PID  PPID  STATE     AGE   NAME\n");
    while (user_ps_get(index, &info) == 0) {
        shell_write_uint(info.pid);
        user_write("    ");
        shell_write_uint(info.ppid);
        user_write("     ");

        if (info.state == 1) {
            user_write("READY");
        } else if (info.state == 2) {
            user_write("RUNNING");
        } else if (info.state == 3) {
            user_write("ZOMBIE");
        } else if (info.state == 4) {
            user_write("BLOCKED");
        } else if (info.state == 5) {
            user_write("SLEEPING");
        } else {
            user_write("UNKNOWN");
        }

        user_write("   ");
        shell_write_uint((int)info.age_ticks);
        user_write("   ");
        user_write(info.name);
        user_write("\n");
        index++;
    }
}

// 兼容保留 sleep <ticks> 和 sleep <pid> <ticks> 两种最小教学语义。
static void shell_cmd_sleep(int argc, char** argv) {
    int first = 0;
    int second = 0;

    if (argc <= 1) {
        user_write("Usage: sleep <ticks>\n");
        return;
    }

    if (argc == 2) {
        if (shell_atoi(argv[1], &first) == 0) {
            user_write("Invalid ticks\n");
            return;
        }

        if (first == 0) {
            return;
        }

        if (user_sleep_ticks((unsigned int)first) < 0) {
            user_write("Sleep failed\n");
        }
        return;
    }

    if (argc == 3) {
        if (shell_atoi(argv[1], &first) == 0 || shell_atoi(argv[2], &second) == 0) {
            user_write("Invalid ticks\n");
            return;
        }

        if (user_sleep_pid(first, (unsigned int)second) < 0) {
            user_write("Sleep failed\n");
        }
        return;
    }

    user_write("Usage: sleep <ticks>\n");
}

// 输出帮助信息。
static void shell_cmd_help(void) {
    user_write("commands:\n");
    user_write("  help\n");
    user_write("  echo <text>\n");
    user_write("  run <program>\n");
    user_write("  start <program>\n");
    user_write("  wait <pid>\n");
    user_write("  kill <pid>\n");
    user_write("  ps    show process table with age ticks\n");
    user_write("  uptime\n");
    user_write("  ticks\n");
    user_write("  sleep <ticks>\n");
    user_write("  hello\n");
    user_write("  exit\n");
}

// 用户态 shell 主循环：保持最小交互式行为即可。
void _start(void) {
    static char line[SHELL_LINE_MAX];
    static char* argv[SHELL_ARGV_MAX];
    static char* hello_argv[] = {"hello"};

    user_write("shell start\n");

    for (;;) {
        int argc;

        user_write("MiniOS$ ");
        shell_read_line(line, SHELL_LINE_MAX);
        argc = shell_split_line(line, argv, SHELL_ARGV_MAX);
        if (argc == 0) {
            continue;
        }

        if (shell_streq(argv[0], "help")) {
            shell_cmd_help();
            continue;
        }

        if (shell_streq(argv[0], "echo")) {
            shell_cmd_echo(argc, argv);
            continue;
        }

        if (shell_streq(argv[0], "ps")) {
            shell_cmd_ps();
            continue;
        }

        if (shell_streq(argv[0], "uptime") || shell_streq(argv[0], "ticks")) {
            shell_cmd_uptime();
            continue;
        }

        if (shell_streq(argv[0], "sleep")) {
            shell_cmd_sleep(argc, argv);
            continue;
        }

        if (shell_streq(argv[0], "wait")) {
            int pid = 0;

            if (argc <= 1) {
                user_write("Usage: wait <pid>\n");
                continue;
            }

            if (shell_atoi(argv[1], &pid) == 0) {
                user_write("Invalid pid\n");
                continue;
            }

            if (user_waitpid(pid) < 0) {
                user_write("Wait failed\n");
                continue;
            }

            user_write("Wait done\n");
            continue;
        }

        if (shell_streq(argv[0], "kill")) {
            int pid = 0;

            if (argc <= 1) {
                user_write("Usage: kill <pid>\n");
                continue;
            }

            if (shell_atoi(argv[1], &pid) == 0) {
                user_write("Invalid pid\n");
                continue;
            }

            if (user_kill(pid) < 0) {
                user_write("Kill failed\n");
                continue;
            }

            user_write("Killed\n");
            continue;
        }

        if (shell_streq(argv[0], "run") || shell_streq(argv[0], "start")) {
            int is_start = shell_streq(argv[0], "start");
            int program_id;

            if (argc <= 1) {
                if (is_start != 0) {
                    user_write("Usage: start <program>\n");
                } else {
                    user_write("Usage: run <program>\n");
                }
                continue;
            }

            program_id = shell_program_id_from_name(argv[1]);
            if (program_id < 0) {
                user_write("Unknown program\n");
                continue;
            }

            shell_spawn_program(program_id, argc - 1, &argv[1], is_start == 0, is_start != 0);
            continue;
        }

        if (shell_streq(argv[0], "hello")) {
            shell_spawn_program(3, 1, hello_argv, 1, 0);
            continue;
        }

        if (shell_streq(argv[0], "exit")) {
            user_write("shell exit\n");
            user_exit(0);
        }

        user_write("Unknown command\n");
    }
}
