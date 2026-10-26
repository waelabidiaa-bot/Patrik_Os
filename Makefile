BUILD = build
ISO_DIR = $(BUILD)/iso

CC = gcc
AS = nasm
LD = ld

CFLAGS = -m32 -std=gnu11 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin -Wall -Wextra -O2
LDFLAGS = -m elf_i386 -T linker.ld

.PHONY: all run clean

all: $(BUILD)/os.iso

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.o: kernel/boot.asm | $(BUILD)
	$(AS) -f elf32 $< -o $@

$(BUILD)/gdt.o: kernel/gdt.asm | $(BUILD)
	$(AS) -f elf32 $< -o $@

$(BUILD)/kernel.o: kernel/kernel.c kernel/terminal.h kernel/idt.h kernel/interrupts.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/multiboot.o: kernel/multiboot.c kernel/multiboot.h kernel/terminal.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/pmm.o: kernel/pmm.c kernel/pmm.h kernel/multiboot.h kernel/terminal.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/paging.o: kernel/paging.c kernel/paging.h kernel/pmm.h kernel/terminal.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/vmm.o: kernel/vmm.c kernel/vmm.h kernel/paging.h kernel/pmm.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/valloc.o: kernel/valloc.c kernel/valloc.h kernel/paging.h kernel/valloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@


$(BUILD)/heap.o: kernel/heap.c kernel/heap.h kernel/pmm.h kernel/terminal.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/terminal.o: kernel/terminal.c kernel/terminal.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/keyboard.o: kernel/keyboard.c kernel/keyboard.h kernel/terminal.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/syscall.o: kernel/syscall.c kernel/syscall.h kernel/user_memory.h kernel/terminal.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/user_memory.o: kernel/user_memory.c kernel/user_memory.h kernel/paging.h kernel/vmm.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@


$(BUILD)/idt.o: kernel/idt.c kernel/idt.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/interrupts.o: kernel/interrupts.c kernel/interrupts.h kernel/terminal.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@



$(BUILD)/interrupts_asm.o: kernel/interrupts.asm | $(BUILD)
	$(AS) -f elf32 $< -o $@

$(BUILD)/kernel.elf: $(BUILD)/boot.o $(BUILD)/gdt.o $(BUILD)/kernel.o $(BUILD)/multiboot.o $(BUILD)/pmm.o $(BUILD)/paging.o $(BUILD)/vmm.o $(BUILD)/valloc.o $(BUILD)/heap.o  $(BUILD)/terminal.o $(BUILD)/idt.o $(BUILD)/interrupts.o $(BUILD)/interrupts_asm.o $(BUILD)/keyboard.o $(BUILD)/syscall.o $(BUILD)/user_memory.o linker.ld
	$(LD) $(LDFLAGS) -o $@ $(BUILD)/boot.o $(BUILD)/gdt.o $(BUILD)/kernel.o $(BUILD)/multiboot.o $(BUILD)/pmm.o $(BUILD)/paging.o $(BUILD)/vmm.o $(BUILD)/valloc.o $(BUILD)/heap.o  $(BUILD)/terminal.o $(BUILD)/idt.o $(BUILD)/interrupts.o $(BUILD)/interrupts_asm.o $(BUILD)/keyboard.o $(BUILD)/syscall.o $(BUILD)/user_memory.o

$(ISO_DIR)/boot/kernel.elf: $(BUILD)/kernel.elf
	mkdir -p $(ISO_DIR)/boot
	cp $< $@

$(BUILD)/os.iso: $(ISO_DIR)/boot/kernel.elf boot/grub/grub.cfg
	mkdir -p $(ISO_DIR)/boot/grub
	cp boot/grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-file --is-x86-multiboot $(ISO_DIR)/boot/kernel.elf
	grub-mkrescue -o $@ $(ISO_DIR)

run: all
	qemu-system-i386 -cdrom $(BUILD)/os.iso -boot d -no-reboot -no-shutdown

clean:
	rm -rf $(BUILD)