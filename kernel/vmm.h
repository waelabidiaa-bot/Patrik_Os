
#ifndef VMM_H
#define VMM_H

#include <stdint.h>

#define VMM_PAGE_SIZE 4096u

#define VMM_OK          0
#define VMM_ERROR      -1
#define VMM_NOT_MAPPED -2
#define VMM_ALREADY_MAPPED -3

int vmm_map_page(uint32_t virtual_address,
                 uint32_t physical_address,
                 uint32_t flags);

int vmm_unmap_page(uint32_t virtual_address);

int vmm_get_physical_address(uint32_t virtual_address,
                             uint32_t *physical_address);

#endif
