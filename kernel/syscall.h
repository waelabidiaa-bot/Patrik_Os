#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

#define SYS_WRITE 1

int32_t syscall_handle(uint32_t number, uint32_t arg1);

#endif