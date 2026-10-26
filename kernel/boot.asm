
BITS 32

section .multiboot
align 4

; Multiboot 1 header
dd 0x1BADB002
dd 0x00000003
dd -(0x1BADB002 + 0x00000003)

section .text
global _start
extern kernel_main
extern gdt_initialize

_start:
    cli

    ; Save the values passed by GRUB immediately.
    mov [multiboot_magic_saved], eax
    mov [multiboot_info_saved], ebx

    ; Initialize our own stack.
    mov esp, stack_top
    and esp, 0xFFFFFFF0

    ; Initialize the GDT.
    call gdt_initialize

    ; kernel_main(magic, info_address)
    ; cdecl: push arguments right-to-left.
    push dword [multiboot_info_saved]
    push dword [multiboot_magic_saved]
    call kernel_main
    add esp, 8

.hang:
    cli
    hlt
    jmp .hang

section .bss
align 16

stack_bottom:
    resb 16384
stack_top:

section .data
align 4

multiboot_magic_saved:
    dd 0

multiboot_info_saved:
    dd 0