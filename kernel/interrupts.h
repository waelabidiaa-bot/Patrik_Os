#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>

struct cpu_registers
{
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t original_esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
};

void interrupts_initialize(void);

void interrupt_handler(uint32_t interrupt_number);

void exception_handler(
    uint32_t vector,
    uint32_t error_code,
    uint32_t eip,
    const struct cpu_registers *regs
);

#endif