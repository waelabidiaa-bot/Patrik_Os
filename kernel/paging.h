
#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

#define PAGE_SIZE 4096u
#define PAGE_ENTRIES 1024u

#define PAGE_PRESENT  0x001u
#define PAGE_WRITABLE 0x002u
#define PAGE_USER     0x004u

void paging_initialize(void);
int paging_is_enabled(void);

/* Access the active page directory. */
uint32_t *paging_get_page_directory(void);

/* Invalidate a virtual address's cached translation. */
void paging_invalidate_page(uint32_t virtual_address);

#endif
