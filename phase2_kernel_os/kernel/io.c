// io.c：实现 x86 端口 IO 的最小封装
#include "io.h"

// 端口 IO 是 CPU 与硬件设备通信的最基础方式
void outb(unsigned short port, unsigned char value) {
    __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
}

// 从指定端口读取 1 字节设备数据
unsigned char inb(unsigned short port) {
    unsigned char value;
    __asm__ __volatile__("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

// 向 0x80 端口写入一次空数据，常用于老式硬件兼容等待
void io_wait(void) {
    __asm__ __volatile__("outb %%al, $0x80" : : "a"(0));
}
