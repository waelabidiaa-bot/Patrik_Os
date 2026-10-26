#ifndef PMM_H
#define PMM_H

#include <stdint.h>

#define PMM_PAGE_SIZE 4096u
#define PMM_ALLOC_FAILED 0xFFFFFFFFu

void pmm_initialize(uint32_t multiboot_magic, uint32_t multiboot_info_address);
uint32_t pmm_alloc_page(void);
void pmm_free_page(uint32_t physical_address);
uint32_t pmm_get_free_pages(void);

#endif