#include "interrupts.h"
#include "terminal.h"
#include "keyboard.h"

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;

    __asm__ volatile (
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

void interrupts_initialize(void)
{
    /* Begin PIC initialization. */
    outb(0x20, 0x11);
    outb(0xA0, 0x11);

    /* Remap master PIC to vectors 32–39. */
    outb(0x21, 0x20);

    /* Remap slave PIC to vectors 40–47. */
    outb(0xA1, 0x28);

    /* Configure master/slave wiring. */
    outb(0x21, 0x04);
    outb(0xA1, 0x02);

    /* Use 8086 mode. */
    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    /* Mask every IRQ except keyboard IRQ1. */
    outb(0x21, 0xFD);
    outb(0xA1, 0xFF);
}

void interrupt_handler(uint32_t interrupt_number)
{
    if (interrupt_number == 33)
    {
        uint8_t scancode = inb(0x60);

        keyboard_handle_scancode(scancode);

        outb(0x20, 0x20);
    }
}

static void print_hex(uint32_t value)
{
    const char hex[] = "0123456789ABCDEF";

    terminal_writestring("0x");

    for (int shift = 28; shift >= 0; shift -= 4)
    {
        terminal_putchar(hex[(value >> shift) & 0xF]);
    }
}

static void print_page_fault_error(uint32_t error_code)
{
    terminal_writestring("\nPage fault reason: ");

    if (error_code & 0x01)
        terminal_writestring("Protection violation");
    else
        terminal_writestring("Page not present");

    terminal_writestring("\nAccess type: ");

    if (error_code & 0x02)
        terminal_writestring("Write");
    else
        terminal_writestring("Read");

    terminal_writestring("\nPrivilege: ");

    if (error_code & 0x04)
        terminal_writestring("User mode");
    else
        terminal_writestring("Supervisor mode");

    if (error_code & 0x08)
        terminal_writestring("\nReserved page-table bit violation");

    if (error_code & 0x10)
        terminal_writestring("\nInstruction fetch");

    terminal_putchar('\n');
}


void exception_handler(
    uint32_t vector,
    uint32_t error_code,
    uint32_t eip,
    const struct cpu_registers *regs)
{

    __asm__ volatile ("cli");

    if (vector == 14) {
        volatile uint16_t *vga = (volatile uint16_t *)0xB8000;

        vga[0] = (uint16_t)(0x4F00 | 'P');
        vga[1] = (uint16_t)(0x4F00 | 'F');
    }
    static const char *exception_names[] = {
        "Divide Error",
        "Debug",
        "Non-Maskable Interrupt",
        "Breakpoint",
        "Overflow",
        "BOUND Range Exceeded",
        "Invalid Opcode",
        "Device Not Available",
        "Double Fault",
        "Coprocessor Segment Overrun",
        "Invalid TSS",
        "Segment Not Present",
        "Stack-Segment Fault",
        "General Protection Fault",
        "Page Fault",
        "Reserved",
        "x87 Floating-Point Exception",
        "Alignment Check",
        "Machine Check",
        "SIMD Floating-Point Exception",
        "Virtualization Exception",
        "Control Protection Exception"
    };

    __asm__ volatile ("cli");

    terminal_set_color(COLOR_LIGHT_RED, COLOR_BLACK);
    terminal_writestring("\n\n*** CPU EXCEPTION ***\n");

    terminal_writestring("Vector: ");
    print_hex(vector);

    terminal_writestring("\nName: ");

    if (vector < sizeof(exception_names) / sizeof(exception_names[0]))
        terminal_writestring(exception_names[vector]);
    else
        terminal_writestring("Unknown Exception");

    terminal_writestring("\nError code: ");
    print_hex(error_code);

    terminal_writestring("\n\n--- General-purpose registers ---\n");

    terminal_writestring("EAX: ");
    print_hex(regs->eax);
    terminal_writestring("  EBX: ");
    print_hex(regs->ebx);

    terminal_writestring("\nECX: ");
    print_hex(regs->ecx);
    terminal_writestring("  EDX: ");
    print_hex(regs->edx);

    terminal_writestring("\nESI: ");
    print_hex(regs->esi);
    terminal_writestring("  EDI: ");
    print_hex(regs->edi);

    terminal_writestring("\nEBP: ");
    print_hex(regs->ebp);
    terminal_writestring("  ESP: ");
    print_hex(regs->original_esp);

    terminal_writestring("\n\nInstruction address (EIP): ");
    print_hex(eip);

    if (vector == 14)
    {
    uint32_t cr2;

    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));

    terminal_writestring("\nPage fault address (CR2): ");
    print_hex(cr2);

    print_page_fault_error(error_code);
    }

    terminal_writestring("\n\nSystem halted.\n");

    for (;;)
        __asm__ volatile ("hlt");
}