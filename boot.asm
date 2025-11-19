; boot.asm - The entry point for the kernel
bits 32
section .multiboot
    dd 0x1BADB002             ; Magic number for multiboot
    dd 0x0                    ; Flags
    dd - (0x1BADB002 + 0x0)   ; Checksum

section .text
global start
extern kernel_main            ; Defined in kernel.c

start:
    cli                       ; Block interrupts
    mov esp, stack_space      ; Set stack pointer
    call kernel_main          ; Jump to C code
    hlt                       ; Halt the CPU

section .bss
resb 8192                     ; Reserve 8KB for stack
stack_space: