NASM = nasm
CC = gcc
LD = ld

BUILD_DIR = build
BOOT_SRC = boot/boot.asm
INTERRUPT_SRC = boot/interrupt.asm
KERNEL_SRC = kernel/kernel.c
IDT_SRC = kernel/idt.c
IO_SRC = kernel/io.c
PIC_SRC = kernel/pic.c
PIT_SRC = kernel/pit.c
VGA_SRC = drivers/vga.c
BOOT_OBJ = $(BUILD_DIR)/boot.o
INTERRUPT_OBJ = $(BUILD_DIR)/interrupt.o
KERNEL_OBJ = $(BUILD_DIR)/kernel.o
IDT_OBJ = $(BUILD_DIR)/idt.o
IO_OBJ = $(BUILD_DIR)/io.o
PIC_OBJ = $(BUILD_DIR)/pic.o
PIT_OBJ = $(BUILD_DIR)/pit.o
VGA_OBJ = $(BUILD_DIR)/vga.o
KERNEL_ELF = $(BUILD_DIR)/kernel.elf

CFLAGS = -m32 -ffreestanding -fno-pie -fno-stack-protector -Wall -Wextra -O0 -Iinclude
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib

.PHONY: all run clean

all: $(KERNEL_ELF)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BOOT_OBJ): $(BOOT_SRC) | $(BUILD_DIR)
	$(NASM) -f elf32 $< -o $@

$(INTERRUPT_OBJ): $(INTERRUPT_SRC) | $(BUILD_DIR)
	$(NASM) -f elf32 $< -o $@

$(KERNEL_OBJ): $(KERNEL_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(IDT_OBJ): $(IDT_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(IO_OBJ): $(IO_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(PIC_OBJ): $(PIC_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(PIT_OBJ): $(PIT_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(VGA_OBJ): $(VGA_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_ELF): $(BOOT_OBJ) $(INTERRUPT_OBJ) $(KERNEL_OBJ) $(IDT_OBJ) $(IO_OBJ) $(PIC_OBJ) $(PIT_OBJ) $(VGA_OBJ) linker.ld
	$(LD) $(LDFLAGS) $(BOOT_OBJ) $(INTERRUPT_OBJ) $(KERNEL_OBJ) $(IDT_OBJ) $(IO_OBJ) $(PIC_OBJ) $(PIT_OBJ) $(VGA_OBJ) -o $@

run: $(KERNEL_ELF)
	qemu-system-i386 -kernel $(KERNEL_ELF)

clean:
	rm -rf $(BUILD_DIR)
