/* ===== shell 主循环 ===== */

// 用户态 shell 主循环：保持最小交互式行为即可。
void _start(void) {
    static char line[SHELL_LINE_MAX];
    static char* argv[SHELL_ARGV_MAX];
    static char* hello_argv[] = {"hello"};

    user_write("shell start\n");

    for (;;) {
        int argc;
        int pipe_index;
        int pipe_status;
        int redirect_index;
        int redirect_is_append;
        int redirect_status;
        int input_redirect_index;
        int input_redirect_status;
        int right_redirect_index;
        int right_redirect_is_append;
        int right_redirect_status;
        int right_effective_argc;
        int left_effective_argc;
        int left_redirect_status;
        struct shell_redirect_info run_redirects;
        struct shell_redirect_info left_pipe_redirects;
        struct shell_redirect_info right_pipe_redirects;

        user_write("MiniOS$ ");
        shell_read_line(line, SHELL_LINE_MAX);
        argc = shell_split_line(line, argv, SHELL_ARGV_MAX);
        if (argc < 0) {
            user_write("Too many args\n");
            continue;
        }
        if (argc == 0) {
            continue;
        }

        pipe_index = -1;
        pipe_status = shell_find_pipe(argc, argv, &pipe_index);
        if (pipe_status < 0) {
            user_write("pipe: only one | is supported\n");
            continue;
        }

        redirect_index = -1;
        redirect_is_append = 0;
        redirect_status = shell_find_redirect(argc, argv, &redirect_index, &redirect_is_append);
        if (redirect_status < 0) {
            user_write("redirect: invalid syntax\n");
            continue;
        }

        input_redirect_index = -1;
        input_redirect_status = shell_find_input_redirect(argc, argv, &input_redirect_index);
        if (input_redirect_status < 0) {
            user_write("redirect: invalid input syntax\n");
            continue;
        }

        if (pipe_index >= 0) {
            if (pipe_index == 0 || (pipe_index + 1) >= argc) {
                user_write("pipe: missing command\n");
                continue;
            }

            if (input_redirect_index > pipe_index) {
                user_write("pipe with right-side input redirection is not supported yet\n");
                continue;
            }

            if (redirect_index >= 0 && redirect_index < pipe_index) {
                user_write("pipe with left-side redirection is not supported yet\n");
                continue;
            }

            left_effective_argc = pipe_index;
            if (input_redirect_index >= 0) {
                left_redirect_status = shell_parse_run_redirects(pipe_index, argv, -1, 0, input_redirect_index, &left_pipe_redirects);
                if (left_redirect_status < 0) {
                    shell_write_run_redirect_error(left_redirect_status);
                    continue;
                }

                if (left_pipe_redirects.first_redirect_index == 1) {
                    user_write("redirect: missing program target\n");
                    continue;
                }

                if (left_pipe_redirects.first_redirect_index > 0) {
                    left_effective_argc = left_pipe_redirects.first_redirect_index;
                }

            } else {
                left_pipe_redirects.has_stdin = 0;
                left_pipe_redirects.stdin_path = (const char*)0;
            }

            right_redirect_index = -1;
            right_redirect_is_append = 0;
            right_effective_argc = argc - pipe_index - 1;
            if (redirect_index >= 0) {
                right_redirect_status = shell_find_redirect(argc - pipe_index - 1, &argv[pipe_index + 1], &right_redirect_index, &right_redirect_is_append);
                if (right_redirect_status < 0) {
                    user_write("redirect: invalid syntax\n");
                    continue;
                }

                right_redirect_status = shell_parse_run_redirects(argc - pipe_index - 1, &argv[pipe_index + 1], right_redirect_index, right_redirect_is_append, -1, &right_pipe_redirects);
                if (right_redirect_status < 0) {
                    shell_write_run_redirect_error(right_redirect_status);
                    continue;
                }

                if (right_pipe_redirects.first_redirect_index == 1) {
                    user_write("redirect: missing program target\n");
                    continue;
                }

                if (right_pipe_redirects.first_redirect_index > 0) {
                    right_effective_argc = right_pipe_redirects.first_redirect_index;
                }
            } else {
                right_pipe_redirects.has_stdout = 0;
                right_pipe_redirects.stdout_append = 0;
                right_pipe_redirects.stdout_path = (const char*)0;
            }

            shell_run_single_pipe(
                left_effective_argc,
                argv,
                right_effective_argc,
                &argv[pipe_index + 1],
                left_pipe_redirects.stdin_path,
                right_pipe_redirects.stdout_path,
                right_pipe_redirects.stdout_append);
            continue;
        }

        if (input_redirect_index >= 0) {
            if (shell_streq(argv[0], "run") == 0) {
                user_write("input redirect only supports run now\n");
                continue;
            }
        }

        if (redirect_index >= 0) {
            if (shell_streq(argv[0], "echo") != 0) {
                if (input_redirect_index >= 0) {
                    user_write("combined redirection only supports run now\n");
                    continue;
                }
                shell_cmd_echo_redirect(argc, argv, redirect_index, redirect_is_append);
                continue;
            }

            if (shell_streq(argv[0], "run") != 0) {
                // run 的重定向留到后面的 run 分支统一处理，避免把 > / >> 误传给用户程序 argv。
            } else {
                user_write("redirect only supports echo/run now\n");
                continue;
            }
        }

        if (shell_streq(argv[0], "help")) {
            shell_cmd_help();
            continue;
        }

        if (shell_streq(argv[0], "echo")) {
            shell_cmd_echo(argc, argv);
            continue;
        }

        if (shell_streq(argv[0], "clear")) {
            // clear 只负责请求内核清屏；下一轮循环会自然重新打印提示符。
            user_clear_screen();
            continue;
        }

        if (shell_streq(argv[0], "ps")) {
            shell_cmd_ps();
            continue;
        }

        if (shell_streq(argv[0], "jobs")) {
            shell_cmd_jobs();
            continue;
        }

        if (shell_streq(argv[0], "ls")) {
            shell_cmd_ls(argc);
            continue;
        }

        if (shell_streq(argv[0], "cat")) {
            shell_cmd_cat(argc, argv);
            continue;
        }

        if (shell_streq(argv[0], "touch")) {
            shell_cmd_touch(argc, argv);
            continue;
        }

        if (shell_streq(argv[0], "writefile")) {
            shell_cmd_writefile(argc, argv);
            continue;
        }

        if (shell_streq(argv[0], "append")) {
            shell_cmd_append(argc, argv);
            continue;
        }

        if (shell_streq(argv[0], "rm")) {
            shell_cmd_rm(argc, argv);
            continue;
        }

        if (shell_streq(argv[0], "fdtest")) {
            shell_cmd_fdtest();
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
            int wait_result;

            if (argc == 1) {
                wait_result = user_wait_any();
                if (wait_result <= 0) {
                    user_write("No exited child\n");
                    continue;
                }

                user_write("Wait done pid: ");
                shell_write_uint(wait_result);
                user_write("\n");
                continue;
            }

            if (argc != 2) {
                user_write("Usage: wait [pid]\n");
                continue;
            }

            if (shell_atoi(argv[1], &pid) == 0) {
                user_write("Invalid pid\n");
                continue;
            }

            wait_result = user_waitpid(pid);
            if (wait_result < 0) {
                user_write("Wait failed\n");
                continue;
            }

            user_write("Wait done pid: ");
            shell_write_uint(wait_result);
            user_write("\n");
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
            int validate_result;
            int program_argc = argc - 1;

            if (argc <= 1) {
                if (is_start != 0) {
                    user_write("Usage: start <program>\n");
                } else {
                    user_write("Usage: run <program>\n");
                }
                continue;
            }

            validate_result = shell_parse_run_redirects(argc, argv, redirect_index, redirect_is_append, input_redirect_index, &run_redirects);
            if (validate_result < 0) {
                shell_write_run_redirect_error(validate_result);
                continue;
            }

            if (run_redirects.first_redirect_index == 1) {
                user_write("redirect: missing program target\n");
                continue;
            }

            if (run_redirects.has_stdout != 0) {
                if (is_start != 0) {
                    user_write("redirect does not support start\n");
                    continue;
                }
            }

            if (run_redirects.has_stdin != 0) {
                if (is_start != 0) {
                    user_write("input redirect does not support start\n");
                    continue;
                }
            }

            if (run_redirects.first_redirect_index > 0) {
                program_argc = run_redirects.first_redirect_index - 1;
            }

            program_id = shell_program_id_from_name(argv[1]);
            if (program_id == PROGRAM_INVALID) {
                user_write("Unknown program\n");
                continue;
            }

            validate_result = shell_validate_program_args(program_argc, &argv[1]);
            if (validate_result == -1) {
                user_write("Too many args\n");
                continue;
            }
            if (validate_result == -2) {
                user_write("Arg too long\n");
                continue;
            }

            shell_spawn_program(
                program_id,
                program_argc,
                &argv[1],
                is_start == 0,
                is_start != 0,
                run_redirects.stdout_path,
                run_redirects.stdout_append,
                run_redirects.stdin_path,
                0,
                0);
            continue;
        }

        if (shell_streq(argv[0], "hello")) {
            shell_spawn_program(PROGRAM_HELLO, 1, hello_argv, 1, 0, (const char*)0, 0, (const char*)0, 0, 0);
            continue;
        }

        if (shell_streq(argv[0], "exit")) {
            user_write("shell exit\n");
            user_exit(0);
        }

        user_write("Unknown command\n");
    }
}
