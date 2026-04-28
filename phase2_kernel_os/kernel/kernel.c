// 内核主函数：在 VGA 文本模式下输出启动成功信息
void kernel_main(void) {
    // VGA 文本模式显存起始地址（80x25）
    volatile unsigned short* vga = (volatile unsigned short*)0xB8000;
    // 要显示的启动成功字符串
    const char* msg = "MiniOS Kernel Boot Success";
    // 前景亮白、背景黑色
    const unsigned char color = 0x0F;

    // 逐字符写入第一行显存：高 8 位颜色，低 8 位字符
    for (int i = 0; msg[i] != '\0'; i++) {
        vga[i] = ((unsigned short)color << 8) | (unsigned char)msg[i];
    }

    // 内核主循环：当前阶段不做调度和中断，仅保持运行
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}
