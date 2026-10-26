#include <stdint.h>

#include "paging.h"
#include "pmm.h"
#include "terminal.h"

#define PAGE_ENTRIES 1024u
#define PAGE_TABLE_SPAN (4u * 1024u * 1024u)
#define IDENTITY_MAP_LIMIT (128u * 1024u * 1024u)
#define CR0_PAGING 0x80000000u

static uint32_t *page_directory;
static int paging_enabled;

static inline uint32_t read_cr0(void)
{
    uint32_t value;

    __asm__ volatile ("mov %%cr0, %0" : "=r"(value));

    return value;
}

static inline void write_cr3(uint32_t value)
{
    __asm__ volatile ("mov %0, %%cr3" :: "r"(value) : "memory");
}

static inline void write_cr0(uint32_t value)
{
    __asm__ volatile ("mov %0, %%cr0" :: "r"(value) : "memory");
}

int paging_is_enabled(void)
{
    return paging_enabled;
}

void paging_initialize(void)
{
    paging_enabled = 0;
    page_directory = 0;

    uint32_t directory_physical = pmm_alloc_page();

    if (directory_physical == PMM_ALLOC_FAILED)
    {
        terminal_writestring("PAGING: Page directory allocation failed.\n");
        return;
    }

    page_directory = (uint32_t *)(uintptr_t)directory_physical;

    for (uint32_t i = 0; i < PAGE_ENTRIES; i++)
        page_directory[i] = 0;

    const uint32_t table_count =
        IDENTITY_MAP_LIMIT / PAGE_TABLE_SPAN;

    for (uint32_t directory_index = 0;
         directory_index < table_count;
         directory_index++)
    {
        uint32_t table_physical = pmm_alloc_page();

        if (table_physical == PMM_ALLOC_FAILED)
        {
            terminal_writestring("PAGING: Page table allocation failed.\n");
            return;
        }

        uint32_t *page_table =
            (uint32_t *)(uintptr_t)table_physical;

        for (uint32_t table_index = 0;
             table_index < PAGE_ENTRIES;
             table_index++)
        {
            uint32_t physical_address =
                directory_index * PAGE_TABLE_SPAN
                + table_index * PAGE_SIZE;

            page_table[table_index] =
                physical_address | PAGE_PRESENT | PAGE_WRITABLE;
        }

        page_directory[directory_index] =
            table_physical | PAGE_PRESENT | PAGE_WRITABLE;
    }

    /*
     * Load the physical address of the page directory.
     */
    write_cr3(directory_physical);

    /*
     * Enable paging. The identity mappings preserve access to
     * addresses in the first 128 MiB.
     */
    write_cr0(read_cr0() | CR0_PAGING);

    paging_enabled = 1;

    terminal_writestring("PAGING: Enabled; first 128 MiB identity-mapped.\n");
}

uint32_t *paging_get_page_directory(void)
{
    return page_directory;
}

void paging_invalidate_page(uint32_t virtual_address)
{
    __asm__ volatile (
        "invlpg (%0)"
        :
        : "r"((void *)(uintptr_t)virtual_address)
        : "memory"
    );
}
