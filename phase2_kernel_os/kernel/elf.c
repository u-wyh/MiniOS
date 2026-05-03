#include "elf.h"
#include "mm.h"
#include "paging.h"

#define PAGE_SIZE 4096

// 内置 test 程序 ELF 镜像：用于 exec("test") 的最小用户程序
static const unsigned char builtin_test_elf_image[] = {
    0x7F, 0x45, 0x4C, 0x46, 0x01, 0x01, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x54, 0x00, 0x40, 0x00, 0x34, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x34, 0x00, 0x20, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,

    0x01, 0x00, 0x00, 0x00, 0x54, 0x00, 0x00, 0x00,
    0x54, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x38, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00,

    0xB8, 0x01, 0x00, 0x00, 0x00,
    0xBB, 0x7C, 0x00, 0x40, 0x00,
    0xCD, 0x80,
    0xB8, 0x03, 0x00, 0x00, 0x00,
    0xCD, 0x80,
    0xB8, 0x04, 0x00, 0x00, 0x00,
    0xCD, 0x80,
    0xBB, 0x00, 0x00, 0x00, 0x00,
    0xB8, 0x02, 0x00, 0x00, 0x00,
    0xCD, 0x80,
    0xEB, 0xFE,

    'H', 'e', 'l', 'l', 'o', ' ', 'f', 'r', 'o', 'm', ' ', 'E', 'L', 'F', '\n', 0x00
};

// 手动读取小端 32 位值，避免依赖平台对齐/类型转换细节
static unsigned int read_u32_le(const unsigned char* p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

// 校验 ELF Header 的关键字段与边界，确保后续解析安全
static int elf_valid_header(const struct Elf32_Ehdr* ehdr, unsigned int elf_size) {
    // 至少要容纳完整 ELF Header
    if (elf_size < sizeof(struct Elf32_Ehdr)) {
        return 0;
    }

    // 校验 ELF 魔数
    if (read_u32_le(ehdr->e_ident) != ELF_MAGIC) {
        return 0;
    }

    // 当前最小实现只支持 32 位小端 ELF
    if (ehdr->e_ident[4] != ELFCLASS32 || ehdr->e_ident[5] != ELFDATA2LSB) {
        return 0;
    }

    // Program Header 项大小必须匹配我们定义的结构
    if (ehdr->e_phentsize != sizeof(struct Elf32_Phdr)) {
        return 0;
    }

    if (ehdr->e_phnum == 0) {
        return 0;
    }

    // Program Header 表必须在 ELF 镜像范围内
    if (ehdr->e_phoff + ((unsigned int)ehdr->e_phnum * sizeof(struct Elf32_Phdr)) > elf_size) {
        return 0;
    }

    return 1;
}

// 把一段内存清零，供段加载时初始化 memsz 区间使用
static void zero_bytes(unsigned char* dst, unsigned int size) {
    unsigned int i;

    for (i = 0; i < size; i++) {
        dst[i] = 0;
    }
}

// 从源地址拷贝固定字节数到目标地址，不依赖标准库 memcpy
static void copy_bytes(unsigned char* dst, const unsigned char* src, unsigned int size) {
    unsigned int i;

    for (i = 0; i < size; i++) {
        dst[i] = src[i];
    }
}

// 记录本次新映射页；若同一页已记录则返回已有下标，避免重复映射造成泄漏
static int elf_track_page(struct elf_load_info* info, unsigned int page_va, unsigned int page_pa, unsigned int page_flags) {
    unsigned int i;

    if (info == (struct elf_load_info*)0) {
        return -1;
    }

    for (i = 0; i < info->page_count; i++) {
        if (info->page_vaddr[i] == page_va) {
            return (int)i;
        }
    }

    if (info->page_count >= ELF_LOAD_MAX_PAGES) {
        return -2;
    }

    info->page_vaddr[info->page_count] = page_va;
    info->page_paddr[info->page_count] = page_pa;
    info->page_flags[info->page_count] = page_flags;
    info->page_count++;
    return (int)(info->page_count - 1);
}

// 加载内存中的最小 ELF：映射 PT_LOAD 段并返回入口地址
unsigned int elf_load(const unsigned char* elf_data, unsigned int elf_size) {
    return elf_load_with_info(elf_data, elf_size, (struct elf_load_info*)0);
}

// 加载内存中的最小 ELF：映射 PT_LOAD 段并返回入口地址，同时可选记录映射页信息
unsigned int elf_load_with_info(const unsigned char* elf_data, unsigned int elf_size, struct elf_load_info* info) {
    const struct Elf32_Ehdr* ehdr;
    const struct Elf32_Phdr* phdr;
    unsigned int i;

    if (info != (struct elf_load_info*)0) {
        info->page_count = 0;
    }

    // 1) 解析并校验 ELF Header
    ehdr = (const struct Elf32_Ehdr*)elf_data;
    if (elf_valid_header(ehdr, elf_size) == 0) {
        return 0;
    }

    // 2) 遍历 Program Header，仅处理 PT_LOAD 段
    phdr = (const struct Elf32_Phdr*)(elf_data + ehdr->e_phoff);
    for (i = 0; i < ehdr->e_phnum; i++) {
        unsigned int seg_start;
        unsigned int seg_end;
        unsigned int page_va;
        unsigned int user_flags;

        // 非可加载段直接跳过
        if (phdr[i].p_type != PT_LOAD) {
            continue;
        }

        if (phdr[i].p_memsz == 0) {
            continue;
        }

        // 最小安全检查：文件段大小不能超过内存段大小
        if (phdr[i].p_filesz > phdr[i].p_memsz) {
            return 0;
        }

        // 段文件内容不能越过 ELF 镜像边界
        if (phdr[i].p_offset + phdr[i].p_filesz > elf_size) {
            return 0;
        }

        // 3) 为段覆盖的每个虚拟页分配物理页并建立映射
        seg_start = phdr[i].p_vaddr;
        seg_end = phdr[i].p_vaddr + phdr[i].p_memsz;
        page_va = seg_start & 0xFFFFF000;

        // 段权限：默认用户可访问；写段额外开放 PAGE_WRITABLE
        user_flags = PAGE_PRESENT | PAGE_USER;
        if ((phdr[i].p_flags & PF_W) != 0) {
            user_flags |= PAGE_WRITABLE;
        }

        while (page_va < seg_end) {
            int tracked;

            // 对同一虚拟页重复出现的段，复用已有映射，避免重复分配物理页
            tracked = -1;
            if (info != (struct elf_load_info*)0) {
                tracked = elf_track_page(info, page_va, 0, user_flags);
                if (tracked >= 0 && info->page_paddr[(unsigned int)tracked] != 0) {
                    page_va += PAGE_SIZE;
                    continue;
                }
            }

            unsigned char* page = (unsigned char*)alloc_page();
            if (page == (unsigned char*)0) {
                return 0;
            }

            map_page(page_va, (unsigned int)page, user_flags);

            if (info != (struct elf_load_info*)0) {
                if (tracked == -1) {
                    tracked = elf_track_page(info, page_va, (unsigned int)page, user_flags);
                } else if (tracked >= 0) {
                    info->page_paddr[(unsigned int)tracked] = (unsigned int)page;
                    info->page_flags[(unsigned int)tracked] = user_flags;
                }

                if (tracked < 0) {
                    // 记录失败时及时回滚当前页，防止因为元信息不足导致泄漏
                    unmap_page(page_va);
                    free_page(page);
                    return 0;
                }
            }

            page_va += PAGE_SIZE;
        }

        // 4) 先清零 memsz，再拷贝 filesz，满足 .bss 语义
        zero_bytes((unsigned char*)phdr[i].p_vaddr, phdr[i].p_memsz);
        copy_bytes((unsigned char*)phdr[i].p_vaddr, elf_data + phdr[i].p_offset, phdr[i].p_filesz);
    }

    // 5) 返回入口地址，供 user 模块切换到 Ring3 执行
    return ehdr->e_entry;
}

// 返回内置 test ELF 的地址与长度，供 exec 模块按名称加载
const unsigned char* elf_get_test_image(unsigned int* elf_size) {
    if (elf_size != (unsigned int*)0) {
        *elf_size = sizeof(builtin_test_elf_image);
    }

    return builtin_test_elf_image;
}
