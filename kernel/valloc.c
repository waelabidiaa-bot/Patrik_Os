#include <stdint.h>
#include <stddef.h>

#include "valloc.h"
#include "pmm.h"
#include "vmm.h"
#include "terminal.h"

#define VALLOC_START 0x10000000u
#define VALLOC_END   0x20000000u

#define VALLOC_MAX_ALLOCATIONS 256u

#define PAGE_MASK (VMM_PAGE_SIZE - 1u)
#define PAGE_ADDRESS_MASK 0xFFFFF000u
#define PAGE_WRITABLE_FLAG 0x002u

typedef struct
{
    uint32_t start;
    uint32_t page_count;
    uint8_t used;
} valloc_record_t;

static valloc_record_t allocations[VALLOC_MAX_ALLOCATIONS];
static uint32_t next_virtual_address = VALLOC_START;
static uint8_t allocator_initialized = 0;

/*
 * Check whether a virtual page is currently unmapped.
 */
static int virtual_page_is_free(uint32_t virtual_address)
{
    uint32_t physical_address;

    return vmm_get_physical_address(
        virtual_address,
        &physical_address
    ) == VMM_NOT_MAPPED;
}

/*
 * Search the allocation table for an unused record.
 */
static int find_free_record(void)
{
    for (uint32_t i = 0; i < VALLOC_MAX_ALLOCATIONS; i++)
    {
        if (!allocations[i].used)
            return (int)i;
    }

    return -1;
}

/*
 * Find a contiguous range of free virtual pages.
 *
 * Searches from next_virtual_address to VALLOC_END,
 * then wraps around and searches from VALLOC_START.
 */
static int find_virtual_region(uint32_t page_count,
                               uint32_t *result)
{
    if (result == 0 || page_count == 0)
        return 0;

    uint32_t region_pages =
        (VALLOC_END - VALLOC_START) / VMM_PAGE_SIZE;

    if (page_count > region_pages)
        return 0;

    uint32_t total_bytes = page_count * VMM_PAGE_SIZE;

    uint32_t start = next_virtual_address;

    if (start < VALLOC_START || start >= VALLOC_END)
        start = VALLOC_START;

    /*
     * Search two ranges:
     * 1. next_virtual_address -> VALLOC_END
     * 2. VALLOC_START -> next_virtual_address
     */
    for (uint32_t pass = 0; pass < 2; pass++)
    {
        uint32_t limit =
            (pass == 0) ? VALLOC_END : next_virtual_address;

        if (pass == 1)
            start = VALLOC_START;

        while (start < limit &&
               total_bytes <= limit - start)
        {
            uint32_t checked = 0;

            while (checked < page_count &&
                   virtual_page_is_free(
                       start + checked * VMM_PAGE_SIZE))
            {
                checked++;
            }

            if (checked == page_count)
            {
                *result = start;
                return 1;
            }

            /*
             * Skip past the occupied page.
             */
            start += (checked + 1u) * VMM_PAGE_SIZE;
        }
    }

    return 0;
}

/*
 * Initialize allocation metadata.
 */
void valloc_initialize(void)
{
    for (uint32_t i = 0; i < VALLOC_MAX_ALLOCATIONS; i++)
    {
        allocations[i].start = 0;
        allocations[i].page_count = 0;
        allocations[i].used = 0;
    }

    next_virtual_address = VALLOC_START;
    allocator_initialized = 1;
}

/*
 * Allocate contiguous virtual memory backed by physical pages.
 *
 * The returned pointer is valid only while paging is enabled.
 */
void *valloc(size_t size)
{
    if (!allocator_initialized)
    {
        terminal_writestring("[VAlloc DEBUG] Not initialized\n");
        return 0;
    }

    if (size == 0)
    {
        terminal_writestring("[VAlloc DEBUG] Size is zero\n");
        return 0;
    }
    if (size > UINT32_MAX - PAGE_MASK)
        return 0;

    int record_index = find_free_record();

    if (record_index < 0)
    {
        terminal_writestring("[VAlloc DEBUG] Allocation table full\n");
        return 0;
    }

    uint32_t page_count =
        ((uint32_t)size + PAGE_MASK) / VMM_PAGE_SIZE;

    uint32_t virtual_start;

    if (!find_virtual_region(page_count, &virtual_start))
    {
        terminal_writestring("[VAlloc DEBUG] No free virtual region\n");
        return 0;
    }

    uint32_t mapped_pages = 0;

    /*
     * Allocate and map physical pages one at a time.
     */
    for (; mapped_pages < page_count; mapped_pages++)
    {
        uint32_t physical_address = pmm_alloc_page();

        if (physical_address == PMM_ALLOC_FAILED)
        {
            terminal_writestring("[VAlloc DEBUG] PMM allocation failed\n");
            break;
        }

        int result = vmm_map_page(
            virtual_start + mapped_pages * VMM_PAGE_SIZE,
            physical_address,
            PAGE_WRITABLE_FLAG
        );

        if (result != VMM_OK)
        {
            terminal_writestring("[VAlloc DEBUG] VMM mapping failed\n");
            break;
        }
    }

    /*
     * Roll back every successful mapping if allocation fails.
     */
    if (mapped_pages != page_count)
    {
        for (uint32_t i = 0; i < mapped_pages; i++)
        {
            uint32_t virtual_address =
                virtual_start + i * VMM_PAGE_SIZE;

            uint32_t physical_address;

            if (vmm_get_physical_address(
                    virtual_address,
                    &physical_address) == VMM_OK)
            {
                if (vmm_unmap_page(virtual_address) == VMM_OK)
                {
                    pmm_free_page(
                        physical_address & PAGE_ADDRESS_MASK
                    );
                }
            }
        }

        return 0;
    }

    allocations[record_index].start = virtual_start;
    allocations[record_index].page_count = page_count;
    allocations[record_index].used = 1;

    next_virtual_address =
        virtual_start + page_count * VMM_PAGE_SIZE;

    if (next_virtual_address >= VALLOC_END)
        next_virtual_address = VALLOC_START;

    return (void *)(uintptr_t)virtual_start;
}

/*
 * Release an allocation created by valloc().
 *
 * This function refuses to free an allocation if any page
 * is already unmapped, avoiding partial freeing in that case.
 */
int vfree(void *pointer)
{
    if (!allocator_initialized || pointer == 0)
        return VALLOC_INVALID;

    uint32_t address = (uint32_t)(uintptr_t)pointer;

    if (address & PAGE_MASK)
        return VALLOC_INVALID;

    int record_index = -1;

    for (uint32_t i = 0; i < VALLOC_MAX_ALLOCATIONS; i++)
    {
        if (allocations[i].used &&
            allocations[i].start == address)
        {
            record_index = (int)i;
            break;
        }
    }

    if (record_index < 0)
        return VALLOC_NOT_FOUND;

    uint32_t page_count = allocations[record_index].page_count;

    /*
     * Verify every mapping before freeing anything.
     */
    for (uint32_t i = 0; i < page_count; i++)
    {
        uint32_t physical_address;

        if (vmm_get_physical_address(
                address + i * VMM_PAGE_SIZE,
                &physical_address) != VMM_OK)
        {
            return VALLOC_ERROR;
        }
    }

    /*
     * Remove mappings and free physical pages.
     */
    for (uint32_t i = 0; i < page_count; i++)
    {
        uint32_t virtual_address =
            address + i * VMM_PAGE_SIZE;

        uint32_t physical_address;

        if (vmm_get_physical_address(
                virtual_address,
                &physical_address) != VMM_OK)
        {
            return VALLOC_ERROR;
        }

        if (vmm_unmap_page(virtual_address) != VMM_OK)
            return VALLOC_ERROR;

        pmm_free_page(
            physical_address & PAGE_ADDRESS_MASK
        );
    }

    allocations[record_index].start = 0;
    allocations[record_index].page_count = 0;
    allocations[record_index].used = 0;

    return VALLOC_OK;
}