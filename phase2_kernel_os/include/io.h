#ifndef IO_H
#define IO_H

// 向指定端口写入 1 字节数据，用于 CPU 与外设通信
void outb(unsigned short port, unsigned char value);
// 从指定端口读取 1 字节数据，用于获取外设状态
unsigned char inb(unsigned short port);
// 进行一次极短的 IO 等待，保证连续端口操作时序稳定
void io_wait(void);

#endif
