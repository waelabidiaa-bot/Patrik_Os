#include <stdint.h>

#include "paging.h"
#include "vmm.h"

#define PAGE_ADDRESS_MASK 0xFFFFF000u
#define PAGE_USER 0x004u
#define PAGE_PRESENT 0x001u

/*
 * Check every page touched by [address, address + length).
 * A readable user page must be present and user-accessible
 * in both its PDE and PTE.
 */
int user_memory_range_readable(uint32_t address, uint32_t length)
{
    if (length == 0)
        return 1;

    /* Reject address-range overflow. */
    if (address > UINT32_MAX - (length - 1u))
        return 0;

    uint32_t last_address = address + length - 1u;
    uint32_t page_address = address & PAGE_ADDRESS_MASK;
    uint32_t last_page = last_address & PAGE_ADDRESS_MASK;

    uint32_t *directory = paging_get_page_directory();

    if (directory == 0 || !paging_is_enabled())
        return 0;

    for (;;)
    {
        uint32_t directory_index = page_address >> 22;
        uint32_t table_index = (page_address >> 12) & 0x3FFu;
        uint32_t pde = directory[directory_index];

        if ((pde & (PAGE_PRESENT | PAGE_USER)) !=
            (PAGE_PRESENT | PAGE_USER))
            return 0;

        uint32_t physical_address;

        if (vmm_get_physical_address(page_address, &physical_address) != VMM_OK)
            return 0;

        /*
         * vmm_get_physical_address verifies the PTE is present.
         * Check the user bit in that PTE as well.
         */
        uint32_t *page_table =
            (uint32_t *)(uintptr_t)(pde & PAGE_ADDRESS_MASK);

        if ((page_table[table_index] & (PAGE_PRESENT | PAGE_USER)) !=
            (PAGE_PRESENT | PAGE_USER))
            return 0;

        if (page_address == last_page)
            break;

        page_address += 4096u;
    }

    return 1;
}