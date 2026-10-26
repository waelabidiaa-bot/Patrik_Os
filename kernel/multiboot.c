
#include "multiboot.h"
#include "terminal.h"

static void print_hex64(uint64_t value)
{
    static const char digits[] = "0123456789ABCDEF";

    terminal_writestring("0x");

    for (int i = 15; i >= 0; i--)
    {
        unsigned int digit = (unsigned int)((value >> (i * 4)) & 0xF);
        terminal_putchar(digits[digit]);
    }
}

void multiboot_print_memory_map(uint32_t magic, uint32_t info_address)
{
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
    {
        terminal_writestring("Invalid Multiboot magic number.\n");
        return;
    }

    const struct multiboot_info *info =
        (const struct multiboot_info *)(uintptr_t)info_address;

    if (!(info->flags & MULTIBOOT_INFO_MMAP))
    {
        terminal_writestring("GRUB did not provide a memory map.\n");
        return;
    }

    terminal_writestring("\nPhysical memory map:\n");

    uint32_t offset = 0;

    while (offset < info->mmap_length)
    {
        const struct multiboot_mmap_entry *entry =
            (const struct multiboot_mmap_entry *)
            (uintptr_t)(info->mmap_addr + offset);

        terminal_writestring("Base: ");
        print_hex64(entry->addr);

        terminal_writestring("  Length: ");
        print_hex64(entry->len);

        terminal_writestring("  Type: ");

        if (entry->type == 1)
            terminal_writestring("Available\n");
        else
            terminal_writestring("Reserved/Other\n");

        if (entry->size < 20)
        {
            terminal_writestring("Invalid memory map entry size.\n");
            return;
        }

        offset += entry->size + sizeof(entry->size);
    }
}
