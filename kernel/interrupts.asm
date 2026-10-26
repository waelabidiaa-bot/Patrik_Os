BITS 32

section .text

    extern interrupt_handler
    extern exception_handler
    extern syscall_handle
    global syscall_interrupt_stub

    ; System call: int 0x80
    ; EAX = syscall number
    ; EBX = first argument
    syscall_interrupt_stub:
        pushad

        ; pushad saves:
        ; [ESP + 28] = original EAX
        ; [ESP + 16] = original EBX
        mov eax, [esp + 28]
        mov edx, [esp + 16]

        ; C calling convention: arguments right to left.
        push edx
        push eax
        call syscall_handle
        add esp, 8

        ; Save the syscall return value into saved EAX.
        mov [esp + 28], eax

        popad
        iretd
    global default_interrupt_stub
    global keyboard_interrupt_stub
    global exception_stub_table

    ; Halt safely for unhandled interrupts.
default_interrupt_stub:
    cli
.hang:
    hlt
    jmp .hang

; Keyboard IRQ1: vector 33.
keyboard_interrupt_stub:
    pushad

    push dword 33
    call interrupt_handler
    add esp, 4

    popad
    iretd

; Exceptions without a CPU-pushed error code.
%macro ISR_NO_ERROR 1
global exception_stub_%1
exception_stub_%1:
    push dword 0
    push dword %1
    jmp exception_common
%endmacro

; Exceptions with a CPU-pushed error code.
%macro ISR_ERROR 1
global exception_stub_%1
exception_stub_%1:
    push dword %1
    jmp exception_common
%endmacro

; Common exception handler.
; Stack before pushad:
; [vector] [error code] [EIP] [CS] [EFLAGS]
exception_common:
    pushad

    ; Save a pointer to the saved registers.
    ; Layout after pushad:
    ; +00 EDI
    ; +04 ESI
    ; +08 EBP
    ; +12 original ESP
    ; +16 EBX
    ; +20 EDX
    ; +24 ECX
    ; +28 EAX
    ; +32 vector
    ; +36 error code
    ; +40 EIP
    ; +44 CS
    ; +48 EFLAGS

    mov eax, esp

    ; C arguments are pushed right to left:
    ; exception_handler(vector, error_code, eip, regs)
    lea edx, [eax + 40]
    push eax                ; Pointer to saved registers
    push dword [edx]        ; EIP
    push dword [eax + 36]   ; Error code
    push dword [eax + 32]   ; Vector

    call exception_handler
    add esp, 16

    popad
    add esp, 8
    iretd

; CPU exceptions 0–31.
ISR_NO_ERROR 0
ISR_NO_ERROR 1
ISR_NO_ERROR 2
ISR_NO_ERROR 3
ISR_NO_ERROR 4
ISR_NO_ERROR 5
ISR_NO_ERROR 6
ISR_NO_ERROR 7
ISR_ERROR    8
ISR_NO_ERROR 9
ISR_ERROR    10
ISR_ERROR    11
ISR_ERROR    12
ISR_ERROR    13
ISR_ERROR    14
ISR_NO_ERROR 15
ISR_NO_ERROR 16
ISR_ERROR    17
ISR_NO_ERROR 18
ISR_NO_ERROR 19
ISR_NO_ERROR 20
ISR_ERROR    21
ISR_NO_ERROR 22
ISR_NO_ERROR 23
ISR_NO_ERROR 24
ISR_NO_ERROR 25
ISR_NO_ERROR 26
ISR_NO_ERROR 27
ISR_NO_ERROR 28
ISR_ERROR    29
ISR_ERROR    30
ISR_NO_ERROR 31

section .rodata
align 4

exception_stub_table:
%assign i 0
%rep 32
    dd exception_stub_%+i
%assign i i+1
%endrep

section .note.GNU-stack noalloc noexec nowrite progbits