#include "idt.h"

#define IDT_ENTRIES 256

#define KERNEL_CODE_SELECTOR 0x08
#define IDT_INTERRUPT_GATE   0x8E

struct idt_entry
{
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t flags;
    uint16_t offset_high;
} __attribute__((packed));

struct idt_pointer
{
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[IDT_ENTRIES];

extern void default_interrupt_stub(void);
extern void keyboard_interrupt_stub(void);
extern void *exception_stub_table[];
extern void syscall_interrupt_stub(void);

static void idt_set_gate(
    uint8_t vector,
    uint32_t handler,
    uint16_t selector,
    uint8_t flags)
{
    idt[vector].offset_low = (uint16_t)(handler & 0xFFFF);
    idt[vector].selector = selector;
    idt[vector].zero = 0;
    idt[vector].flags = flags;
    idt[vector].offset_high = (uint16_t)((handler >> 16) & 0xFFFF);
}

void idt_initialize(void)
{
    /* Give every vector a valid default handler first. */
    for (uint32_t i = 0; i < IDT_ENTRIES; i++)
    {
        idt_set_gate(
            (uint8_t)i,
            (uint32_t)default_interrupt_stub,
            KERNEL_CODE_SELECTOR,
            IDT_INTERRUPT_GATE
        );
    }

    /* Install the real CPU exception handlers (vectors 0–31). */
    for (uint32_t i = 0; i < 32; i++)
    {
        idt_set_gate(
            (uint8_t)i,
            (uint32_t)exception_stub_table[i],
            KERNEL_CODE_SELECTOR,
            IDT_INTERRUPT_GATE
        );
    }

    /* Keyboard IRQ1 uses interrupt vector 33. */
    idt_set_gate(
        33,
        (uint32_t)keyboard_interrupt_stub,
        KERNEL_CODE_SELECTOR,
        IDT_INTERRUPT_GATE
    );
    /* System calls are callable from Ring 3: DPL = 3. */
    idt_set_gate(
        0x80,
        (uint32_t)(uintptr_t)syscall_interrupt_stub,
        KERNEL_CODE_SELECTOR,
        0xEE
    );

    struct idt_pointer idtr;
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint32_t)(uintptr_t)idt;

    __asm__ volatile ("lidt %0" : : "m"(idtr));
}