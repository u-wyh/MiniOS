/* ===== shell 交互输入与程序启动辅助 ===== */

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

// 按空格或 tab 拆分参数，直接在原始缓冲区上写入 '\0'；若 token 数超过上限则返回 -1。
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
            return -1;
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

// 通过共享用户程序清单做 name -> program_id 解析，避免 shell 再维护一份散落映射。
static int shell_program_id_from_name(const char* name) {
#define SHELL_MATCH_PROGRAM(symbol, value, program_name, visible) \
    if ((visible) != 0 && shell_streq(name, program_name)) {      \
        return symbol;                                             \
    }
    MINIOS_USER_PROGRAM_LIST(SHELL_MATCH_PROGRAM)
#undef SHELL_MATCH_PROGRAM

    // 兼容不便输入下划线的场景，允许用 sleeptest 作为 sleep_test 的简写别名。
    if (shell_streq(name, "sleeptest")) {
        return PROGRAM_SLEEP_TEST;
    }

    // 兼容不便输入下划线的场景，允许用 loopexit 作为 loop_exit 的简写别名。
    if (shell_streq(name, "loopexit")) {
        return PROGRAM_LOOP_EXIT;
    }

    return PROGRAM_INVALID;
}

// 校验即将传给用户程序的教学版 argv：当前保留 argv[0]=程序名，参数过多或过长时在 shell 侧先失败。
static int shell_validate_program_args(int argc, char** argv) {
    int i;

    if (argc < 0 || argc > USER_PROGRAM_MAX_ARGS) {
        return -1;
    }

    for (i = 0; i < argc; i++) {
        if (shell_string_length_with_limit(argv[i], USER_PROGRAM_MAX_ARG_LEN) < 0) {
            return -2;
        }
    }

    return 0;
}

// 统一处理 run/start/hello 的 fork + exec + waitpid 逻辑；可按需为子进程配置文件重定向或教学版 pipe 输入输出。
static int shell_spawn_program(int program_id, int argc, char** argv, int wait_child, int is_background, const char* stdout_redirect_path, int stdout_redirect_is_append, const char* stdin_redirect_path, int stdout_to_pipe, int stdin_from_pipe) {
    int pid = user_fork();

    if (pid < 0) {
        user_write("Fork failed\n");
        return -1;
    }

    if (pid == 0) {
        // 子进程先等待父进程补齐 stdin/stdout/pipe 配置，再真正 exec 到目标程序。
        // 这样可以避免 run + 重定向/管道 场景里出现“子进程先跑、父进程后挂配置”的竞态。
        while (user_get_launch_ready() == 0) {
            user_yield();
        }

        int exec_result = user_exec_args(program_id, argc, (const char* const*)argv);

        if (exec_result != 0) {
            user_write("Exec failed\n");
            user_exit(99);
        }
        user_exit(0);
    }

    if (is_background != 0) {
        user_set_background(pid, 1);
    }

    if (stdout_redirect_path != (const char*)0) {
        if (user_set_stdout_redirect(pid, stdout_redirect_path, stdout_redirect_is_append) < 0) {
            user_kill(pid);
            if (wait_child != 0) {
                user_waitpid(pid);
            }
            user_write("Redirect setup failed\n");
            return -1;
        }
    }

    if (stdout_to_pipe != 0) {
        if (user_set_stdout_pipe(pid) < 0) {
            user_kill(pid);
            if (wait_child != 0) {
                user_waitpid(pid);
            }
            user_write("pipe stdout setup failed\n");
            return -1;
        }
    }

    if (stdin_redirect_path != (const char*)0) {
        if (user_set_stdin_redirect(pid, stdin_redirect_path) < 0) {
            user_kill(pid);
            if (wait_child != 0) {
                user_waitpid(pid);
            }
            user_write("stdin redirect setup failed\n");
            return -1;
        }
    }

    if (stdin_from_pipe != 0) {
        if (user_set_stdin_pipe(pid) < 0) {
            user_kill(pid);
            if (wait_child != 0) {
                user_waitpid(pid);
            }
            user_write("pipe stdin setup failed\n");
            return -1;
        }
    }

    if (user_set_launch_ready(pid) < 0) {
        user_kill(pid);
        if (wait_child != 0) {
            user_waitpid(pid);
        }
        user_write("launch setup failed\n");
        return -1;
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

