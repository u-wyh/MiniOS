/* ===== 用户态 shell 的共享静态数据 ===== */

// 用户态 shell 侧的教学版只读文件视图：直接复用共享宏定义，避免 ls/cat 各自维护散落文件名。
static const struct shell_builtin_text_file shell_builtin_files[] = {
#define SHELL_BUILD_TEXT_FILE(path_text, content_text) \
    {path_text, content_text, (unsigned int)(sizeof(content_text) - 1)},
    MINIOS_BUILTIN_TEXT_FILE_LIST(SHELL_BUILD_TEXT_FILE)
#undef SHELL_BUILD_TEXT_FILE
};

