/* ===== shell 管道执行模型 ===== */

// 执行教学版单管道：当前采用“左侧先完整运行并写 pipe buffer，右侧再读取 pipe buffer”的顺序模型。
// Task83 之后，shell 侧的 pipe 接线语义统一为：
// 1. 左侧子进程内部优先执行 fd_dup2(pipe_write_fd, 1)
// 2. 右侧子进程内部优先执行 fd_dup2(pipe_read_fd, 0)
static void shell_run_single_pipe(int left_argc, char** left_argv, int right_argc, char** right_argv, const char* left_stdin_path, const char* right_stdout_path, int right_stdout_is_append) {
    int left_program_id;
    int right_program_id;
    int validate_result;

    if (left_argc < 2 || right_argc < 2) {
        user_write("pipe: missing command\n");
        return;
    }

    if (shell_streq(left_argv[0], "run") == 0 || shell_streq(right_argv[0], "run") == 0) {
        user_write("pipe only supports run ... | run ...\n");
        return;
    }

    left_program_id = shell_program_id_from_name(left_argv[1]);
    right_program_id = shell_program_id_from_name(right_argv[1]);
    if (left_program_id == PROGRAM_INVALID || right_program_id == PROGRAM_INVALID) {
        user_write("Unknown program\n");
        return;
    }

    validate_result = shell_validate_program_args(left_argc - 1, &left_argv[1]);
    if (validate_result == -1) {
        user_write("Too many args\n");
        return;
    }
    if (validate_result == -2) {
        user_write("Arg too long\n");
        return;
    }

    validate_result = shell_validate_program_args(right_argc - 1, &right_argv[1]);
    if (validate_result == -1) {
        user_write("Too many args\n");
        return;
    }
    if (validate_result == -2) {
        user_write("Arg too long\n");
        return;
    }

    if (left_stdin_path != (const char*)0) {
        validate_result = shell_validate_stdin_source(left_stdin_path);
        if (validate_result < 0) {
            user_write("pipe input source invalid\n");
            return;
        }
    }

    if (right_stdout_path != (const char*)0) {
        validate_result = shell_validate_stdout_target(right_stdout_path, right_stdout_is_append);
        if (validate_result == -2) {
            user_write("pipe redirect target is readonly\n");
            return;
        }
        if (validate_result == -3) {
            user_write("pipe append target must exist\n");
            return;
        }
        if (validate_result < 0) {
            user_write("pipe redirect target invalid\n");
            return;
        }
    }

    user_pipe_reset();
    if (shell_spawn_program(left_program_id, left_argc - 1, &left_argv[1], 1, 0, (const char*)0, 0, left_stdin_path, 1, 0) < 0) {
        user_pipe_reset();
        return;
    }

    if (shell_spawn_program(right_program_id, right_argc - 1, &right_argv[1], 1, 0, right_stdout_path, right_stdout_is_append, (const char*)0, 0, 1) < 0) {
        user_pipe_reset();
        return;
    }

    user_pipe_reset();
}
