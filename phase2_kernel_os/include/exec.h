#ifndef EXEC_H
#define EXEC_H

// 最小程序表项：程序名 + 内置 ELF 镜像地址
struct program {
    const char* name;
    void* elf_data;
};

// 按程序名执行用户程序，当前通过 program table 查找内置 ELF
void exec(const char* name);

#endif
