#include <stdint.h>
#include <stddef.h>

#include "heap.h"
#include "pmm.h"
#include "terminal.h"

#define HEAP_PAGE_SIZE PMM_PAGE_SIZE
#define HEAP_MAGIC 0x50415452u

typedef struct heap_block
{
    size_t size;
    uint32_t magic;
    int free;
    struct heap_block *next;
} heap_block_t;

static heap_block_t *first_block;
static int heap_ready;

static size_t align_size(size_t size)
{
    if (size > SIZE_MAX - 7u)
        return 0;

    return (size + 7u) & ~(size_t)7u;
}

static void initialize_block(heap_block_t *block,
                             size_t size,
                             heap_block_t *next)
{
    block->size = size;
    block->magic = HEAP_MAGIC;
    block->free = 1;
    block->next = next;
}

/*
 * Add one physical page to the heap.
 *
 * This assumes paging is disabled and physical memory is
 * directly addressable, as in your current kernel.
 */
static int heap_add_page(void)
{
    uint32_t physical = pmm_alloc_page();

    if (physical == PMM_ALLOC_FAILED)
        return 0;

    heap_block_t *block = (heap_block_t *)(uintptr_t)physical;

    initialize_block(
        block,
        HEAP_PAGE_SIZE - sizeof(heap_block_t),
        NULL
    );

    if (first_block == NULL)
    {
        first_block = block;
        return 1;
    }

    heap_block_t *current = first_block;

    while (current->next != NULL)
    {
        if (current->magic != HEAP_MAGIC)
        {
            pmm_free_page(physical);
            return 0;
        }

        current = current->next;
    }

    if (current->magic != HEAP_MAGIC)
    {
        pmm_free_page(physical);
        return 0;
    }

    current->next = block;
    return 1;
}

void heap_initialize(void)
{
    first_block = NULL;
    heap_ready = 0;

    if (!heap_add_page())
    {
        terminal_writestring("HEAP: could not allocate initial page.\n");
        return;
    }

    heap_ready = 1;

    terminal_writestring("HEAP: initialized successfully.\n");
}

void *kmalloc(size_t size)
{
    if (!heap_ready || size == 0)
        return NULL;

    size = align_size(size);

    if (size == 0)
        return NULL;

    while (1)
    {
        if (size > HEAP_PAGE_SIZE - sizeof(heap_block_t))
            return NULL;
        heap_block_t *block = first_block;

        while (block != NULL)
        {
            if (block->magic != HEAP_MAGIC)
                return NULL;

            if (block->free && block->size >= size)
            {
                size_t remaining = block->size - size;

                if (remaining >= sizeof(heap_block_t) + 8u)
                {
                    heap_block_t *new_block =
                        (heap_block_t *)((uint8_t *)(block + 1) + size);

                    new_block->size =
                        remaining - sizeof(heap_block_t);
                    new_block->magic = HEAP_MAGIC;
                    new_block->free = 1;
                    new_block->next = block->next;

                    block->next = new_block;
                    block->size = size;
                }

                block->free = 0;
                return (void *)(block + 1);
            }

            block = block->next;
        }

        /*
         * No block fits. Add another page and retry.
         */
        if (!heap_add_page())
            return NULL;
    }
}

void kfree(void *pointer)
{
    if (!heap_ready || pointer == NULL)
        return;

    uintptr_t address = (uintptr_t)pointer;

    heap_block_t *block = (heap_block_t *)pointer - 1;
    uintptr_t header_address = (uintptr_t)block;

    /*
     * Validate the pointer against actual heap blocks before
     * trusting the allocation header.
     */
    heap_block_t *current = first_block;

    while (current != NULL)
    {
        if (current->magic != HEAP_MAGIC)
            return;

        if ((uintptr_t)current == header_address)
            break;

        current = current->next;
    }

    if (current == NULL || (uintptr_t)(current + 1) != address)
        return;

    if (current->free)
        return;

    current->free = 1;

    /*
     * Merge adjacent free blocks only when they are physically
     * contiguous. Separate pages must not be merged across gaps.
     */
    current = first_block;

    while (current != NULL && current->next != NULL)
    {
        heap_block_t *next = current->next;

        if (current->magic != HEAP_MAGIC ||
            next->magic != HEAP_MAGIC)
            return;

        uintptr_t end_of_current =
            (uintptr_t)(current + 1) + current->size;

        if (current->free &&
            next->free &&
            end_of_current == (uintptr_t)next)
        {
            current->size += sizeof(heap_block_t) + next->size;
            current->next = next->next;
        }
        else
        {
            current = next;
        }
    }
}