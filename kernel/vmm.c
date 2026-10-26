
#include <stdint.h>

#include "vmm.h"
#include "paging.h"
#include "pmm.h"

#define PAGE_ADDRESS_MASK 0xFFFFF000u
#define PAGE_OFFSET_MASK  0x00000FFFu
#define PAGE_DIRECTORY_ENTRIES 1024u
#define IDENTITY_MAP_LIMIT (128u * 1024u * 1024u)

/*
 * 1 = the VMM allocated this page table.
 * 0 = the VMM does not own this page table.
 */
static uint8_t owned_page_tables[PAGE_DIRECTORY_ENTRIES] = {0};

/*
 * Convert a physical address to a pointer.
 * The first 128 MiB is identity-mapped by our current paging setup.
 */
static uint32_t *physical_to_pointer(uint32_t physical_address)
{
    if (physical_address >= IDENTITY_MAP_LIMIT)
        return 0;

    return (uint32_t *)(uintptr_t)physical_address;
}

static int page_table_is_empty(uint32_t *page_table)
{
    for (uint32_t i = 0; i < PAGE_ENTRIES; i++)
    {
        if (page_table[i] & PAGE_PRESENT)
            return 0;
    }

    return 1;
}

/*
 * Get a page table, optionally creating one.
 */
static uint32_t *get_page_table(uint32_t directory_index, int create)
{
    uint32_t *directory = paging_get_page_directory();

    if (directory == 0 || directory_index >= PAGE_DIRECTORY_ENTRIES)
        return 0;

    uint32_t entry = directory[directory_index];

    if ((entry & PAGE_PRESENT) == 0)
    {
        if (!create)
            return 0;

        uint32_t table_physical = pmm_alloc_page();

        if (table_physical == PMM_ALLOC_FAILED)
            return 0;

        uint32_t *table = physical_to_pointer(table_physical);

        if (table == 0)
        {
            pmm_free_page(table_physical);
            return 0;
        }

        for (uint32_t i = 0; i < PAGE_ENTRIES; i++)
            table[i] = 0;

        directory[directory_index] =
            table_physical | PAGE_PRESENT | PAGE_WRITABLE;

        owned_page_tables[directory_index] = 1;

        return table;
    }

    return physical_to_pointer(entry & PAGE_ADDRESS_MASK);
}

/*
 * Map one virtual page to an existing physical page.
 */

int vmm_map_page(uint32_t virtual_address,
                 uint32_t physical_address,
                 uint32_t flags)
{
    if (!paging_is_enabled())
        return VMM_ERROR;

    /* Both addresses must be page-aligned. */
    if ((virtual_address & PAGE_OFFSET_MASK) != 0 ||
        (physical_address & PAGE_OFFSET_MASK) != 0)
        return VMM_ERROR;

    uint32_t directory_index = virtual_address >> 22;
    uint32_t table_index = (virtual_address >> 12) & 0x3FFu;

    uint32_t *directory = paging_get_page_directory();

    if (directory == 0)
        return VMM_ERROR;

    uint32_t *table = get_page_table(directory_index, 1);

    if (table == 0)
        return VMM_ERROR;

    /* Do not overwrite an existing mapping. */
    if (table[table_index] & PAGE_PRESENT)
        return VMM_ALREADY_MAPPED;

    /*
     * Set the page-table entry permissions.
     * PAGE_USER is granted only when explicitly requested.
     */
    table[table_index] =
        (physical_address & PAGE_ADDRESS_MASK) |
        PAGE_PRESENT |
        (flags & (PAGE_WRITABLE | PAGE_USER));

    /*
     * A user page requires PAGE_USER in BOTH the
     * page-directory entry and the page-table entry.
     */
    if (flags & PAGE_USER)
        directory[directory_index] |= PAGE_USER;

    paging_invalidate_page(virtual_address);

    return VMM_OK;
}

/*
 * Unmap one virtual page.
 *
 * This function does not free the mapped physical page.
 * The caller remains responsible for that page.
 */
int vmm_unmap_page(uint32_t virtual_address)
{
    if (!paging_is_enabled())
        return VMM_ERROR;

    if (virtual_address & PAGE_OFFSET_MASK)
        return VMM_ERROR;

    uint32_t *directory = paging_get_page_directory();

    if (directory == 0)
        return VMM_ERROR;

    uint32_t directory_index = virtual_address >> 22;
    uint32_t table_index = (virtual_address >> 12) & 0x3FFu;

    if ((directory[directory_index] & PAGE_PRESENT) == 0)
        return VMM_NOT_MAPPED;

    uint32_t table_physical =
        directory[directory_index] & PAGE_ADDRESS_MASK;

    uint32_t *page_table = physical_to_pointer(table_physical);

    if (page_table == 0)
        return VMM_ERROR;

    /*
     * Check that the requested page is actually mapped.
     */
    if ((page_table[table_index] & PAGE_PRESENT) == 0)
        return VMM_NOT_MAPPED;

    /*
     * FIX: clear the requested page-table entry.
     */
    page_table[table_index] = 0;

    /*
     * Invalidate the virtual address in the TLB.
     */
    paging_invalidate_page(virtual_address);

    /*
     * If other pages still use this table, keep it.
     */
    if (!page_table_is_empty(page_table))
        return VMM_OK;

    /*
     * Never free a page table that the VMM does not own.
     */
    if (!owned_page_tables[directory_index])
        return VMM_OK;

    /*
     * The table is empty and owned by the VMM.
     * Remove its directory entry.
     */
    directory[directory_index] = 0;
    owned_page_tables[directory_index] = 0;

    /*
     * Reload CR3 to flush TLB entries associated with this
     * page directory before releasing the page-table memory.
     */
    uint32_t directory_physical = (uint32_t)(uintptr_t)directory;

    __asm__ volatile (
        "mov %0, %%cr3"
        :
        : "r"(directory_physical)
        : "memory"
    );

    pmm_free_page(table_physical);

    return VMM_OK;
}

/*
 * Translate a virtual address into a physical address,
 * preserving the offset within the page.
 */
int vmm_get_physical_address(uint32_t virtual_address,
                             uint32_t *physical_address)
{
    if (!paging_is_enabled() || physical_address == 0)
        return VMM_ERROR;

    uint32_t directory_index = virtual_address >> 22;
    uint32_t table_index = (virtual_address >> 12) & 0x3FFu;
    uint32_t offset = virtual_address & PAGE_OFFSET_MASK;

    uint32_t *table = get_page_table(directory_index, 0);

    if (table == 0)
        return VMM_NOT_MAPPED;

    uint32_t entry = table[table_index];

    if ((entry & PAGE_PRESENT) == 0)
        return VMM_NOT_MAPPED;

    *physical_address =
        (entry & PAGE_ADDRESS_MASK) | offset;

    return VMM_OK;
}
