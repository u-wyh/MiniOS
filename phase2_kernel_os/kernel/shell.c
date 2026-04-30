#include "mm.h"
#include "panic.h"
#include "pit.h"
#include "shell.h"
#include "vga.h"

// 用一个最小地址栈记录 shell 分配过的页，便于连续执行多次 free
#define SHELL_PAGE_HISTORY 128
static void* allocated_pages[SHELL_PAGE_HISTORY];
static int allocated_page_count = 0;
// kmalloc 也维护一份最小地址栈，便于验证 kfree 后的小块复用效果
static void* allocated_blocks[SHELL_PAGE_HISTORY];
static int allocated_block_count = 0;

// 打印统一命令提示符，便于用户看到下一次输入位置
static void shell_print_prompt(void) {
    print_string("MiniOS> ");
}

// 比较两个字符串是否完全相等，不依赖 libc 的 strcmp
static int str_equal(const char* a, const char* b) {
    int i = 0;

    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }

    return a[i] == '\0' && b[i] == '\0';
}

// 判断字符串是否以指定前缀开头，用于识别 echo 命令
static int str_starts_with(const char* s, const char* prefix) {
    int i = 0;

    while (prefix[i] != '\0') {
        if (s[i] != prefix[i]) {
            return 0;
        }
        i++;
    }

    return 1;
}

// 跳过指定前缀，返回命令参数起始位置
static const char* skip_prefix(const char* s, const char* prefix) {
    int i = 0;

    while (prefix[i] != '\0') {
        i++;
    }

    return s + i;
}

// 打印无符号整数，用于在裸机环境下输出 tick 等数值
static void print_uint(unsigned int value) {
    char digits[16];
    int index = 0;

    if (value == 0) {
        print_char('0');
        return;
    }

    while (value > 0) {
        digits[index++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (index > 0) {
        index--;
        print_char(digits[index]);
    }
}

// 打印 32 位地址，便于观察页分配返回的物理页起始位置
static void print_hex(unsigned int value) {
    static const char hex_digits[] = "0123456789ABCDEF";
    int shift;

    print_string("0x");
    for (shift = 28; shift >= 0; shift -= 4) {
        print_char(hex_digits[(value >> shift) & 0xF]);
    }
}

// Shell 初始化：当前阶段只需要打印首个提示符
void shell_init(void) {
    shell_print_prompt();
}

// 执行最小命令集合：help / clear / echo / about / tick / panic / mem / alloc / free / kmalloc / kfree
void shell_execute(const char* line) {
    void* page;
    void* block;

    if (line[0] == '\0') {
        shell_print_prompt();
        return;
    }

    if (str_equal(line, "help")) {
        print_string("help  - show command list\n");
        print_string("clear - clear screen\n");
        print_string("test  - reserved command slot\n");
        print_string("about - show kernel info\n");
        print_string("tick  - show pit ticks\n");
        print_string("panic - trigger kernel panic\n");
        print_string("mem   - show page stats\n");
        print_string("alloc - allocate one page\n");
        print_string("free  - free last page\n");
        print_string("kmalloc - allocate one small block\n");
        print_string("kfree   - free last small block\n");
        print_string("echo  - print text\n");
        shell_print_prompt();
        return;
    }

    if (str_equal(line, "clear")) {
        clear_screen();
        shell_print_prompt();
        return;
    }

    if (str_starts_with(line, "echo ")) {
        print_string(skip_prefix(line, "echo "));
        print_char('\n');
        shell_print_prompt();
        return;
    }

    if (str_equal(line, "about")) {
        print_string("MiniOS Phase2 Kernel\n");
        print_string("Version: 0.1\n");
        print_string("Mode: Protected Mode\n");
        print_string("Features: VGA, GDT, IDT, PIT, Keyboard, Kernel Monitor\n");
        shell_print_prompt();
        return;
    }

    if (str_equal(line, "tick")) {
        print_string("tick: ");
        print_uint(pit_get_ticks());
        print_char('\n');
        shell_print_prompt();
        return;
    }

    if (str_equal(line, "panic")) {
        kernel_panic("manual panic triggered");
        return;
    }

    if (str_equal(line, "mem")) {
        mem_stat();
        shell_print_prompt();
        return;
    }

    if (str_equal(line, "alloc")) {
        page = alloc_page();
        if (page == (void*)0) {
            print_string("alloc failed\n");
        } else {
            // 只要历史栈还有空间，就把新页地址压栈，供后续连续 free 使用
            if (allocated_page_count < SHELL_PAGE_HISTORY) {
                allocated_pages[allocated_page_count] = page;
                allocated_page_count++;
            }
            print_string("alloc page: ");
            print_hex((unsigned int)page);
            print_char('\n');
        }
        shell_print_prompt();
        return;
    }

    if (str_equal(line, "free")) {
        if (allocated_page_count == 0) {
            print_string("no page to free\n");
        } else {
            // 采用后进先出释放方式，让 shell 可以连续回收多次 alloc 得到的页
            allocated_page_count--;
            page = allocated_pages[allocated_page_count];
            free_page(page);
            print_string("free page: ");
            print_hex((unsigned int)page);
            print_char('\n');
        }
        shell_print_prompt();
        return;
    }

    if (str_equal(line, "kmalloc")) {
        block = kmalloc(32);
        if (block == (void*)0) {
            print_string("kmalloc failed\n");
        } else {
            // 用和页分配相同的地址栈方式，支持连续多次 kfree
            if (allocated_block_count < SHELL_PAGE_HISTORY) {
                allocated_blocks[allocated_block_count] = block;
                allocated_block_count++;
            }
            print_string("kmalloc block: ");
            print_hex((unsigned int)block);
            print_char('\n');
        }
        shell_print_prompt();
        return;
    }

    if (str_equal(line, "kfree")) {
        if (allocated_block_count == 0) {
            print_string("no block to free\n");
        } else {
            allocated_block_count--;
            block = allocated_blocks[allocated_block_count];
            kfree(block);
            print_string("kfree block: ");
            print_hex((unsigned int)block);
            print_char('\n');
        }
        shell_print_prompt();
        return;
    }

    print_string("Unknown command\n");
    shell_print_prompt();
}
