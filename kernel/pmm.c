#include <stdint.h>
#include <stddef.h>

#include "pmm.h"
#include "multiboot.h"
#include "terminal.h"

#define MAX_PHYSICAL_MEMORY (128u * 1024u * 1024u)
#define MAX_PAGES (MAX_PHYSICAL_MEMORY / PMM_PAGE_SIZE)
#define BITMAP_SIZE (MAX_PAGES / 8u)

extern uint8_t kernel_start;
extern uint8_t kernel_end;

/*
 * Each bit represents one physical page.
 *
 * 1 = reserved or unavailable
 * 0 = available
 */
static uint8_t bitmap[BITMAP_SIZE];

/*
 * Tracks pages allocated through pmm_alloc_page().
 * This prevents pmm_free_page() from freeing arbitrary reserved pages.
 */
static uint8_t allocated_bitmap[BITMAP_SIZE];

static uint32_t free_pages;
static int pmm_ready;

static void mark_page_used(uint32_t page)
{
    bitmap[page / 8u] |= (uint8_t)(1u << (page % 8u));
}

static void mark_page_free(uint32_t page)
{
    bitmap[page / 8u] &= (uint8_t)~(1u << (page % 8u));
}

static int page_is_used(uint32_t page)
{
    return (bitmap[page / 8u] & (uint8_t)(1u << (page % 8u))) != 0;
}

static void mark_allocated(uint32_t page)
{
    allocated_bitmap[page / 8u] |= (uint8_t)(1u << (page % 8u));
}

static void mark_not_allocated(uint32_t page)
{
    allocated_bitmap[page / 8u] &= (uint8_t)~(1u << (page % 8u));
}

static int page_was_allocated(uint32_t page)
{
    return (allocated_bitmap[page / 8u] &
            (uint8_t)(1u << (page % 8u))) != 0;
}

static void reserve_region(uint64_t start, uint64_t length)
{
    if (length == 0)
        return;

    uint64_t end = start + length;

    if (end < start || start >= MAX_PHYSICAL_MEMORY)
        return;

    if (end > MAX_PHYSICAL_MEMORY)
        end = MAX_PHYSICAL_MEMORY;

    uint32_t first_page = (uint32_t)(start / PMM_PAGE_SIZE);
    uint32_t last_page =
        (uint32_t)((end + PMM_PAGE_SIZE - 1u) / PMM_PAGE_SIZE);

    for (uint32_t page = first_page; page < last_page; page++)
    {
        if (!page_is_used(page))
        {
            mark_page_used(page);
            free_pages--;
        }
    }
}

void pmm_initialize(uint32_t multiboot_magic,
                    uint32_t multiboot_info_address)
{
    pmm_ready = 0;

    /* Initially, treat every page as reserved. */
    for (uint32_t i = 0; i < BITMAP_SIZE; i++)
    {
        bitmap[i] = 0xFF;
        allocated_bitmap[i] = 0;
    }

    free_pages = 0;

    if (multiboot_magic != MULTIBOOT_BOOTLOADER_MAGIC)
    {
        terminal_writestring(
            "PMM: invalid Multiboot magic; allocation disabled.\n");
        return;
    }

    if (multiboot_info_address == 0)
    {
        terminal_writestring(
            "PMM: null Multiboot information address.\n");
        return;
    }

    const struct multiboot_info *info =
        (const struct multiboot_info *)(uintptr_t)multiboot_info_address;

    if (!(info->flags & MULTIBOOT_INFO_MMAP))
    {
        terminal_writestring(
            "PMM: no memory map; allocation disabled.\n");
        return;
    }

    if (info->mmap_length == 0 || info->mmap_addr == 0)
    {
        terminal_writestring(
            "PMM: invalid memory map; allocation disabled.\n");
        return;
    }

    uint32_t offset = 0;

    while (offset < info->mmap_length)
    {
        if (info->mmap_length - offset < sizeof(uint32_t))
        {
            terminal_writestring(
                "PMM: truncated memory map; allocation disabled.\n");
            return;
        }

        uint32_t entry_address = info->mmap_addr + offset;

        if (entry_address < info->mmap_addr)
        {
            terminal_writestring(
                "PMM: memory map address overflow.\n");
            return;
        }

        const struct multiboot_mmap_entry *entry =
            (const struct multiboot_mmap_entry *)(uintptr_t)entry_address;

        uint32_t entry_size = entry->size;

        if (entry_size < 20u ||
            entry_size > info->mmap_length - offset - sizeof(uint32_t))
        {
            terminal_writestring(
                "PMM: malformed memory map entry.\n");
            return;
        }

        if (entry->type == 1u && entry->len != 0)
        {
            uint64_t start = entry->addr;
            uint64_t end = start + entry->len;

            if (end >= start && start < MAX_PHYSICAL_MEMORY)
            {
                if (end > MAX_PHYSICAL_MEMORY)
                    end = MAX_PHYSICAL_MEMORY;

                /* Only free complete pages entirely inside usable RAM. */
                uint64_t aligned_start =
                    (start + PMM_PAGE_SIZE - 1u) &
                    ~((uint64_t)PMM_PAGE_SIZE - 1u);

                uint64_t aligned_end =
                    end & ~((uint64_t)PMM_PAGE_SIZE - 1u);

                if (aligned_start < aligned_end)
                {
                    uint32_t first_page =
                        (uint32_t)(aligned_start / PMM_PAGE_SIZE);

                    uint32_t last_page =
                        (uint32_t)(aligned_end / PMM_PAGE_SIZE);

                    for (uint32_t page = first_page;
                         page < last_page;
                         page++)
                    {
                        if (page_is_used(page))
                        {
                            mark_page_free(page);
                            free_pages++;
                        }
                    }
                }
            }
        }

        offset += entry_size + sizeof(uint32_t);
    }

    /* Reserve the first MiB. */
    reserve_region(0, 0x100000u);

    /* Reserve the kernel image, including its stack and bitmaps. */
    uint64_t kernel_begin = (uint64_t)(uintptr_t)&kernel_start;
    uint64_t kernel_finish = (uint64_t)(uintptr_t)&kernel_end;

    if (kernel_finish > kernel_begin)
    {
        reserve_region(kernel_begin, kernel_finish - kernel_begin);
    }
    else
    {
        terminal_writestring(
            "PMM: invalid kernel boundaries; allocation disabled.\n");
        return;
    }

    /* Reserve the Multiboot information structure. */
    reserve_region(multiboot_info_address,
                   sizeof(struct multiboot_info));

    /* Reserve the memory-map buffer supplied by GRUB. */
    reserve_region(info->mmap_addr, info->mmap_length);

    pmm_ready = 1;

    terminal_writestring("PMM: initialization successful.\n");
}

uint32_t pmm_alloc_page(void)
{
    if (!pmm_ready)
        return PMM_ALLOC_FAILED;

    for (uint32_t page = 256u; page < MAX_PAGES; page++)
    {
        if (!page_is_used(page))
        {
            mark_page_used(page);
            mark_allocated(page);
            free_pages--;

            return page * PMM_PAGE_SIZE;
        }
    }

    return PMM_ALLOC_FAILED;
}

void pmm_free_page(uint32_t physical_address)
{
    if (!pmm_ready)
        return;

    if (physical_address == PMM_ALLOC_FAILED)
        return;

    if (physical_address % PMM_PAGE_SIZE != 0)
        return;

    uint32_t page = physical_address / PMM_PAGE_SIZE;

    if (page >= MAX_PAGES || page < 256u)
        return;

    /* Only free pages allocated through pmm_alloc_page(). */
    if (!page_was_allocated(page))
        return;

    mark_not_allocated(page);
    mark_page_free(page);
    free_pages++;
}

uint32_t pmm_get_free_pages(void)
{
    return free_pages;
}