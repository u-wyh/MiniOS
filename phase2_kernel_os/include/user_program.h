// user_program.h：统一管理 MiniOS Phase2 内置用户程序的 program_id 与描述表
#ifndef USER_PROGRAM_H
#define USER_PROGRAM_H

#include <stdint.h>

// 统一的内置用户程序清单：同时给 shell 名字解析、program_id 管理和内核镜像查询复用。
// shell_visible 为 1 表示允许在用户态 shell 中通过 run/start 直接启动。
#define MINIOS_USER_PROGRAM_LIST(X)                              \
    X(PROGRAM_EXEC_CHILD, 1, "execchild", 0)                    \
    X(PROGRAM_SHELL, 2, "shell", 0)                             \
    X(PROGRAM_HELLO, 3, "hello", 1)                             \
    X(PROGRAM_ECHO, 4, "echo", 1)                               \
    X(PROGRAM_LOOP, 5, "loop", 1)                               \
    X(PROGRAM_SLEEP_TEST, 6, "sleep_test", 1)                   \
    X(PROGRAM_INIT, 7, "init", 0)                               \
    X(PROGRAM_LOOP_EXIT, 8, "loop_exit", 1)                     \
    X(PROGRAM_INFO, 9, "info", 0)                               \
    X(PROGRAM_FORK, 10, "fork", 0)                              \
    X(PROGRAM_FORKEXEC, 11, "forkexec", 0)

// program_id：统一表达内置用户程序身份，避免在 shell / exec / process 里散落魔法数字。
enum user_program_id {
    // 非法 program_id：供查找失败与参数校验返回。
    PROGRAM_INVALID = 0,
#define MINIOS_DECLARE_PROGRAM_ID(symbol, value, name, shell_visible) symbol = value,
    MINIOS_USER_PROGRAM_LIST(MINIOS_DECLARE_PROGRAM_ID)
#undef MINIOS_DECLARE_PROGRAM_ID
    // program_id 上界：便于做范围检查，不代表真实程序项。
    PROGRAM_COUNT
};

// 用户程序描述符：统一记录编号、名称、镜像地址和大小，供内核按 program_id 装载。
struct user_program_descriptor {
    int program_id;
    const char* name;
    const unsigned char* image;
    uint32_t image_size;
    int shell_visible;
};

// 按 program_id 读取描述符；非法编号返回空指针。
const struct user_program_descriptor* user_program_get_by_id(int program_id);
// 按程序名查找描述符；找不到返回空指针。
const struct user_program_descriptor* user_program_find_by_name(const char* name);
// 读取 program_id 对应的规范程序名；失败返回空指针。
const char* user_program_name(int program_id);
// 判断 program_id 是否对应一个有效内置程序。
int user_program_is_valid(int program_id);
// 判断 program_id 是否允许在用户态 shell 中通过 run/start 直接启动。
int user_program_is_shell_runnable(int program_id);

#endif
