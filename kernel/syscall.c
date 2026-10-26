#include <stdint.h>

#include "syscall.h"
#include "terminal.h"
#include "user_memory.h"

#define SYS_WRITE_MAX_LENGTH 256u

#define SYSCALL_OK    0
#define SYSCALL_ERROR (-1)

/*
 * Initial syscall interface:
 *   EAX = syscall number
 *   EBX = argument 1
 *
 * SYS_WRITE:
 *   EBX = pointer to a null-terminated user string
 *
 * Returns 0 on success, -1 on invalid memory or an
 * unterminated string within the permitted length.
 */
int32_t syscall_handle(uint32_t number, uint32_t arg1)
{
    if (number != SYS_WRITE)
        return SYSCALL_ERROR;

    uint32_t address = arg1;

    for (uint32_t i = 0; i < SYS_WRITE_MAX_LENGTH; i++)
    {
        /* Prevent pointer arithmetic from wrapping around. */
        if (address > UINT32_MAX - i)
            return SYSCALL_ERROR;

        uint32_t current = address + i;

        /* Validate the byte before dereferencing it. */
        if (!user_memory_range_readable(current, 1))
            return SYSCALL_ERROR;

        char c = *(const char *)(uintptr_t)current;

        if (c == '\0')
            return SYSCALL_OK;

        terminal_putchar(c);
    }

    return SYSCALL_ERROR;
}