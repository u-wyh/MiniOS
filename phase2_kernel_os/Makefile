NASM = nasm
CC = gcc
LD = ld

BUILD_DIR = build
BOOT_SRC = boot/boot.asm
KERNEL_SRC = kernel/kernel.c
VGA_SRC = drivers/vga.c
BOOT_OBJ = $(BUILD_DIR)/boot.o
KERNEL_OBJ = $(BUILD_DIR)/kernel.o
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

$(KERNEL_OBJ): $(KERNEL_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(VGA_OBJ): $(VGA_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_ELF): $(BOOT_OBJ) $(KERNEL_OBJ) $(VGA_OBJ) linker.ld
	$(LD) $(LDFLAGS) $(BOOT_OBJ) $(KERNEL_OBJ) $(VGA_OBJ) -o $@

run: $(KERNEL_ELF)
	qemu-system-i386 -kernel $(KERNEL_ELF)

clean:
	rm -rf $(BUILD_DIR)
