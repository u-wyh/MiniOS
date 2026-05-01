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
KEYBOARD_SRC = kernel/keyboard.c
SHELL_SRC = kernel/shell.c
PANIC_SRC = kernel/panic.c
TASK_SRC = kernel/task.c
SCHED_SRC = kernel/sched.c
MM_SRC = kernel/mm.c
PAGING_SRC = kernel/paging.c
USER_SRC = kernel/user.c
ELF_SRC = kernel/elf.c
FS_SRC = kernel/fs.c
EXEC_SRC = kernel/exec.c
SYSCALL_SRC = kernel/syscall.c
VGA_SRC = drivers/vga.c
SWITCH_SRC = boot/switch.asm
BOOT_OBJ = $(BUILD_DIR)/boot.o
INTERRUPT_OBJ = $(BUILD_DIR)/interrupt.o
KERNEL_OBJ = $(BUILD_DIR)/kernel.o
IDT_OBJ = $(BUILD_DIR)/idt.o
IO_OBJ = $(BUILD_DIR)/io.o
PIC_OBJ = $(BUILD_DIR)/pic.o
PIT_OBJ = $(BUILD_DIR)/pit.o
KEYBOARD_OBJ = $(BUILD_DIR)/keyboard.o
SHELL_OBJ = $(BUILD_DIR)/shell.o
PANIC_OBJ = $(BUILD_DIR)/panic.o
TASK_OBJ = $(BUILD_DIR)/task.o
SCHED_OBJ = $(BUILD_DIR)/sched.o
MM_OBJ = $(BUILD_DIR)/mm.o
PAGING_OBJ = $(BUILD_DIR)/paging.o
USER_OBJ = $(BUILD_DIR)/user.o
ELF_OBJ = $(BUILD_DIR)/elf.o
FS_OBJ = $(BUILD_DIR)/fs.o
EXEC_OBJ = $(BUILD_DIR)/exec.o
SYSCALL_OBJ = $(BUILD_DIR)/syscall.o
VGA_OBJ = $(BUILD_DIR)/vga.o
SWITCH_OBJ = $(BUILD_DIR)/switch.o
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

$(SWITCH_OBJ): $(SWITCH_SRC) | $(BUILD_DIR)
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

$(KEYBOARD_OBJ): $(KEYBOARD_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(SHELL_OBJ): $(SHELL_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(PANIC_OBJ): $(PANIC_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TASK_OBJ): $(TASK_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(SCHED_OBJ): $(SCHED_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(MM_OBJ): $(MM_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(PAGING_OBJ): $(PAGING_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(USER_OBJ): $(USER_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(ELF_OBJ): $(ELF_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(FS_OBJ): $(FS_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(EXEC_OBJ): $(EXEC_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(SYSCALL_OBJ): $(SYSCALL_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(VGA_OBJ): $(VGA_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_ELF): $(BOOT_OBJ) $(INTERRUPT_OBJ) $(SWITCH_OBJ) $(KERNEL_OBJ) $(IDT_OBJ) $(IO_OBJ) $(PIC_OBJ) $(PIT_OBJ) $(KEYBOARD_OBJ) $(SHELL_OBJ) $(PANIC_OBJ) $(TASK_OBJ) $(SCHED_OBJ) $(MM_OBJ) $(PAGING_OBJ) $(USER_OBJ) $(ELF_OBJ) $(FS_OBJ) $(EXEC_OBJ) $(SYSCALL_OBJ) $(VGA_OBJ) linker.ld
	$(LD) $(LDFLAGS) $(BOOT_OBJ) $(INTERRUPT_OBJ) $(SWITCH_OBJ) $(KERNEL_OBJ) $(IDT_OBJ) $(IO_OBJ) $(PIC_OBJ) $(PIT_OBJ) $(KEYBOARD_OBJ) $(SHELL_OBJ) $(PANIC_OBJ) $(TASK_OBJ) $(SCHED_OBJ) $(MM_OBJ) $(PAGING_OBJ) $(USER_OBJ) $(ELF_OBJ) $(FS_OBJ) $(EXEC_OBJ) $(SYSCALL_OBJ) $(VGA_OBJ) -o $@

run: $(KERNEL_ELF)
	qemu-system-i386 -kernel $(KERNEL_ELF)

clean:
	rm -rf $(BUILD_DIR)
