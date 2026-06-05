/* ===== shell 管道执行模型 ===== */

// 校验每一段教学版 shell pipeline 都是 run 开头，并构造传给 mini_pipeline 的 argv。
// 这里会去掉每段开头的 run，把 | 换成 --，同时剥离首段输入重定向和末段输出重定向 token。
static int shell_build_mini_pipeline_argv(
    int argc,
    char** argv,
    int pipe_count,
    const struct shell_redirect_info* first_redirects,
    const struct shell_redirect_info* last_redirects,
    char** out_argv,
    int max_argv) {
    static char mini_pipeline_name[] = "mini_pipeline";
    static char separator_token[] = "--";
    int out_argc = 0;
    int token_index;
    int current_pipe_index = 0;
    int segment_start = 1;

    if (out_argv == (char**)0 || max_argv <= 0) {
        return -1;
    }

    out_argv[out_argc++] = mini_pipeline_name;

    for (token_index = 0; token_index < argc; token_index++) {
        if (shell_is_pipe_token(argv[token_index]) != 0) {
            if (segment_start != 0) {
                return -2;
            }
            if (out_argc >= max_argv) {
                return -3;
            }

            out_argv[out_argc++] = separator_token;
            current_pipe_index++;
            segment_start = 1;
            continue;
        }

        if (first_redirects != (const struct shell_redirect_info*)0) {
            if (first_redirects->has_stdin != 0 && token_index == first_redirects->stdin_index) {
                token_index++;
                continue;
            }
        }

        if (last_redirects != (const struct shell_redirect_info*)0) {
            if (last_redirects->has_stdout != 0 && token_index == last_redirects->stdout_index) {
                token_index++;
                continue;
            }
        }

        if (segment_start != 0) {
            if (shell_streq(argv[token_index], "run") == 0) {
                return -4;
            }
            segment_start = 0;
            continue;
        }

        if (out_argc >= max_argv) {
            return -3;
        }
        out_argv[out_argc++] = argv[token_index];
    }

    if (segment_start != 0 || current_pipe_index != pipe_count) {
        return -2;
    }

    return out_argc;
}

// 校验多级 pipeline 的 redirect 布局：当前只允许首段输入重定向和末段输出重定向。
static int shell_validate_pipeline_redirect_layout(
    int argc,
    char** argv,
    int pipe_count,
    const int* pipe_indices,
    int input_redirect_index,
    int output_redirect_index) {
    int token_index;

    if (pipe_count <= 0 || pipe_indices == (const int*)0) {
        return -1;
    }

    if (input_redirect_index >= 0 && input_redirect_index > pipe_indices[0]) {
        return -2;
    }

    if (output_redirect_index >= 0 && output_redirect_index < pipe_indices[pipe_count - 1]) {
        return -3;
    }

    for (token_index = 0; token_index < argc; token_index++) {
        if (shell_is_input_redirect_token(argv[token_index]) != 0) {
            if (token_index > pipe_indices[0]) {
                return -2;
            }
            token_index++;
            continue;
        }

        if (shell_is_redirect_token(argv[token_index]) != 0) {
            if (token_index < pipe_indices[pipe_count - 1]) {
                return -3;
            }
            token_index++;
        }
    }

    return 0;
}

// 执行教学版多级 shell pipeline：shell 只负责解析多段 run ... | run ...，真正的数据流复用用户态 mini_pipeline。
static void shell_run_pipeline(
    int argc,
    char** argv,
    int pipe_count,
    int* pipe_indices,
    const char* stdin_path,
    const char* stdout_path,
    int stdout_append) {
    static char* mini_pipeline_argv[SHELL_ARGV_MAX];
    struct shell_redirect_info first_redirects;
    struct shell_redirect_info last_redirects;
    int first_output_index = -1;
    int first_output_is_append = 0;
    int first_input_index = -1;
    int first_redirect_status;
    int last_output_index = -1;
    int last_output_is_append = 0;
    int last_input_index = -1;
    int last_redirect_status;
    int first_argc;
    int last_argc;
    int mini_argc;
    int validate_result;

    (void)stdin_path;
    (void)stdout_path;
    (void)stdout_append;

    if (pipe_count <= 0 || pipe_indices == (int*)0) {
        user_write("pipe: missing command\n");
        return;
    }

    if (pipe_indices[0] == 0 || pipe_indices[pipe_count - 1] == (argc - 1)) {
        user_write("pipe: missing command\n");
        return;
    }

    first_argc = pipe_indices[0];
    last_argc = argc - pipe_indices[pipe_count - 1] - 1;

    first_redirect_status = shell_find_redirect(first_argc, argv, &first_output_index, &first_output_is_append);
    if (first_redirect_status < 0) {
        user_write("redirect: invalid syntax\n");
        return;
    }
    first_redirect_status = shell_find_input_redirect(first_argc, argv, &first_input_index);
    if (first_redirect_status < 0) {
        user_write("redirect: invalid input syntax\n");
        return;
    }

    last_redirect_status = shell_find_redirect(last_argc, &argv[pipe_indices[pipe_count - 1] + 1], &last_output_index, &last_output_is_append);
    if (last_redirect_status < 0) {
        user_write("redirect: invalid syntax\n");
        return;
    }
    last_redirect_status = shell_find_input_redirect(last_argc, &argv[pipe_indices[pipe_count - 1] + 1], &last_input_index);
    if (last_redirect_status < 0) {
        user_write("redirect: invalid input syntax\n");
        return;
    }

    if (first_output_index >= 0) {
        user_write("pipe with left-side output redirection is not supported yet\n");
        return;
    }
    if (last_input_index >= 0) {
        user_write("pipe with right-side input redirection is not supported yet\n");
        return;
    }

    validate_result = shell_validate_pipeline_redirect_layout(
        argc,
        argv,
        pipe_count,
        pipe_indices,
        first_input_index,
        (last_output_index >= 0) ? (pipe_indices[pipe_count - 1] + 1 + last_output_index) : -1);
    if (validate_result == -2) {
        user_write("pipe with right-side input redirection is not supported yet\n");
        return;
    }
    if (validate_result == -3) {
        user_write("pipe with middle output redirection is not supported yet\n");
        return;
    }

    first_redirect_status = shell_parse_run_redirects(first_argc, argv, -1, 0, first_input_index, &first_redirects);
    if (first_redirect_status < 0) {
        shell_write_run_redirect_error(first_redirect_status);
        return;
    }

    if (last_output_index >= 0) {
        last_redirect_status = shell_parse_run_redirects(last_argc, &argv[pipe_indices[pipe_count - 1] + 1], last_output_index, last_output_is_append, -1, &last_redirects);
        if (last_redirect_status < 0) {
            shell_write_run_redirect_error(last_redirect_status);
            return;
        }
    } else {
        last_redirects.has_stdin = 0;
        last_redirects.stdin_index = -1;
        last_redirects.stdin_path = (const char*)0;
        last_redirects.has_stdout = 0;
        last_redirects.stdout_index = -1;
        last_redirects.stdout_append = 0;
        last_redirects.stdout_path = (const char*)0;
        last_redirects.first_redirect_index = -1;
    }

    if (first_redirects.has_stdin != 0) {
        validate_result = shell_validate_stdin_source(first_redirects.stdin_path);
        if (validate_result < 0) {
            user_write("pipe input source invalid\n");
            return;
        }
    }

    if (last_redirects.has_stdout != 0) {
        validate_result = shell_validate_stdout_target(last_redirects.stdout_path, last_redirects.stdout_append);
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

    mini_argc = shell_build_mini_pipeline_argv(argc, argv, pipe_count, &first_redirects, &last_redirects, mini_pipeline_argv, SHELL_ARGV_MAX);
    if (mini_argc < 0) {
        if (mini_argc == -4) {
            user_write("pipe only supports run ... | run ...\n");
        } else if (mini_argc == -3) {
            user_write("Too many args\n");
        } else {
            user_write("pipe: missing command\n");
        }
        return;
    }

    validate_result = shell_validate_program_args(mini_argc, mini_pipeline_argv);
    if (validate_result == -1) {
        user_write("Too many args\n");
        return;
    }
    if (validate_result == -2) {
        user_write("Arg too long\n");
        return;
    }

    shell_spawn_program(
        PROGRAM_MINI_PIPELINE,
        mini_argc,
        mini_pipeline_argv,
        1,
        0,
        last_redirects.stdout_path,
        last_redirects.stdout_append,
        first_redirects.stdin_path,
        0,
        0);
}

// 兼容旧名字：二段 pipe 现在也统一走多级 pipeline 入口。
static void shell_run_single_pipe(int left_argc, char** left_argv, int right_argc, char** right_argv, const char* left_stdin_path, const char* right_stdout_path, int right_stdout_is_append) {
    static char* pipeline_argv[SHELL_ARGV_MAX];
    static char separator_token[] = "|";
    int index;
    int argc = 0;
    int pipe_indices[1];

    for (index = 0; index < left_argc; index++) {
        pipeline_argv[argc++] = left_argv[index];
    }
    pipeline_argv[argc++] = separator_token;
    for (index = 0; index < right_argc; index++) {
        pipeline_argv[argc++] = right_argv[index];
    }

    pipe_indices[0] = left_argc;
    shell_run_pipeline(argc, pipeline_argv, 1, pipe_indices, left_stdin_path, right_stdout_path, right_stdout_is_append);
}
