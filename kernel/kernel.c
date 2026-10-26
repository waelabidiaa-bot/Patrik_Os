#include <stdint.h>

#include "terminal.h"

#include "idt.h"
#include "interrupts.h"
#include "keyboard.h"
#include "pmm.h"
#include "heap.h"
#include "paging.h"
#include "vmm.h"
#include "valloc.h"

/*
 * Test user-page permissions while still running in Ring 0.
 * This function does not enter Ring 3.
 */
static void test_user_page(void)
{
    terminal_writestring("\nTEST: User-accessible page...\n");

    const uint32_t user_virtual_address = 0x30000000u;

    uint32_t physical_address = pmm_alloc_page();

    if (physical_address == PMM_ALLOC_FAILED) {
        terminal_writestring("FAIL: Physical page allocation failed.\n");
        return;
    }

    int result = vmm_map_page(
        user_virtual_address,
        physical_address,
        PAGE_USER | PAGE_WRITABLE
    );

    if (result != VMM_OK) {
        terminal_writestring("FAIL: User-page mapping failed.\n");
        pmm_free_page(physical_address);
        return;
    }

    uint32_t *directory = paging_get_page_directory();

    if (directory == 0) {
        terminal_writestring("FAIL: Page directory unavailable.\n");
        goto cleanup;
    }

    uint32_t directory_index = user_virtual_address >> 22;
    uint32_t directory_entry = directory[directory_index];

    if ((directory_entry & PAGE_USER) == 0) {
        terminal_writestring("FAIL: Directory lacks PAGE_USER.\n");
        goto cleanup;
    }

    uint32_t *page_table = (uint32_t *)(uintptr_t)
        (directory_entry & 0xFFFFF000u);

    uint32_t table_index =
        (user_virtual_address >> 12) & 0x3FFu;

    uint32_t page_entry = page_table[table_index];

    if ((page_entry & PAGE_PRESENT) == 0 ||
        (page_entry & PAGE_USER) == 0 ||
        (page_entry & PAGE_WRITABLE) == 0) {
        terminal_writestring("FAIL: Incorrect page permissions.\n");
        goto cleanup;
    }

    volatile uint32_t *test_address =
        (volatile uint32_t *)(uintptr_t)user_virtual_address;

    *test_address = 0x12345678u;

    if (*test_address != 0x12345678u) {
        terminal_writestring("FAIL: User-page read/write failed.\n");
        goto cleanup;
    }

    terminal_writestring(
        "PASS: User-page permissions and read/write work.\n"
    );

cleanup:
    vmm_unmap_page(user_virtual_address);
    pmm_free_page(physical_address);
}

void kernel_main(
    uint32_t multiboot_magic,
    uint32_t multiboot_info_address
)
{
    terminal_initialize();

    terminal_writestring("\n");
    terminal_writestring("==============================\n");
    terminal_writestring("          PatrikOS\n");
    terminal_writestring("==============================\n");

    terminal_writestring("KERNEL: Loading GDT...\n");
    

    terminal_writestring("KERNEL: Loading IDT...\n");
    idt_initialize();

    terminal_writestring("KERNEL: Initializing interrupts...\n");
    interrupts_initialize();

    terminal_writestring("KERNEL: Initializing keyboard...\n");
    keyboard_initialize();

    terminal_writestring("KERNEL: Initializing physical memory...\n");
    pmm_initialize(multiboot_magic, multiboot_info_address);

    terminal_writestring("KERNEL: Initializing heap...\n");
    heap_initialize();

    terminal_writestring("KERNEL: Initializing paging...\n");
    paging_initialize();

    if (!paging_is_enabled()) {
        terminal_writestring("FATAL: Paging initialization failed.\n");

        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }

    valloc_initialize();

    terminal_writestring(
        "KERNEL: Memory management initialized.\n"
    );

    test_user_page();

    terminal_writestring("\nKERNEL: Initialization complete.\n");
    terminal_writestring("Type on your keyboard.\n");

    /*
     * Enable CPU interrupts so keyboard IRQ1 can be serviced.
     */
    __asm__ volatile ("sti");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
