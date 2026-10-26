BITS 32

section .data
align 8

gdt_start:
    dq 0x0000000000000000 ; 0x00: Null descriptor

    dq 0x00CF9A000000FFFF ; 0x08: Kernel code, Ring 0
    dq 0x00CF92000000FFFF ; 0x10: Kernel data, Ring 0

    dq 0x00CFFA000000FFFF ; 0x18: User code, Ring 3
    dq 0x00CFF2000000FFFF ; 0x20: User data, Ring 3

gdt_tss:
    dq 0                    ; 0x28: TSS descriptor, initialized below

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

section .text
global gdt_initialize

gdt_initialize:
    ; ----------------------------------------
    ; Initialize the 32-bit TSS structure.
    ; ----------------------------------------

    mov dword [tss + 4], tss_kernel_stack_top ; ESP0
    mov word  [tss + 8], 0x10                ; SS0: kernel data
    mov word  [tss + 102], 104               ; I/O map offset

    ; ----------------------------------------
    ; Build the 8-byte TSS descriptor.
    ; Limit = 103 (104 bytes - 1)
    ; Access = 0x89 (present, available 32-bit TSS, Ring 0)
    ; ----------------------------------------

    mov eax, tss

    mov word [gdt_tss], 103       ; Limit bits 0-15
    mov word [gdt_tss + 2], ax    ; Base bits 0-15

    shr eax, 16
    mov byte [gdt_tss + 4], al    ; Base bits 16-23
    mov byte [gdt_tss + 5], 0x89  ; Access byte
    mov byte [gdt_tss + 6], 0x00  ; Limit high bits and flags
    mov byte [gdt_tss + 7], ah    ; Base bits 24-31

    ; Load the GDT.
    lgdt [gdt_descriptor]

    ; Reload the code segment.
    jmp 0x08:.reload_cs

.reload_cs:
    ; Reload kernel data segments.
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Load the Task Register with the TSS selector.
    mov ax, 0x28
    ltr ax

    ret

section .bss
alignb 16

tss:
    resb 104

alignb 16

tss_kernel_stack_bottom:
    resb 16384

tss_kernel_stack_top:

section .note.GNU-stack noalloc noexec nowrite progbits
