; =============================================================================
; AstraOS - x86 (i686) Boot Entry
; =============================================================================
; This is the first code that executes after GRUB hands off control.
; It sets up the Multiboot header, initializes a stack, and jumps to the
; C kernel entry point.
;
; Multiboot 1 specification is used for maximum GRUB compatibility.
; =============================================================================

section .multiboot
align 4

; ---------------------------------------------------------------------------
; Multiboot Header
; ---------------------------------------------------------------------------
; GRUB scans the first 8KB of the kernel image for this magic signature.
; The header must be 4-byte aligned.

MULTIBOOT_MAGIC     equ 0x1BADB002
MULTIBOOT_ALIGN     equ 1 << 0          ; Align modules on page boundaries
MULTIBOOT_MEMINFO   equ 1 << 1          ; Request memory map from GRUB
MULTIBOOT_FLAGS     equ MULTIBOOT_ALIGN | MULTIBOOT_MEMINFO
MULTIBOOT_CHECKSUM  equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

dd MULTIBOOT_MAGIC
dd MULTIBOOT_FLAGS
dd MULTIBOOT_CHECKSUM

; ---------------------------------------------------------------------------
; Kernel Stack
; ---------------------------------------------------------------------------
; We allocate a 16KB stack in BSS. The stack grows downward on x86.
; 16KB is sufficient for early boot; we'll switch to a proper kernel stack
; once memory management is initialized in Phase 2.

section .bss
align 16
stack_bottom:
    resb 16384              ; 16 KB stack
stack_top:

; ---------------------------------------------------------------------------
; Entry Point
; ---------------------------------------------------------------------------
; GRUB jumps here. EAX contains the multiboot magic number (0x2BADB002),
; EBX contains a pointer to the multiboot information structure.
; We must NOT rely on BIOS interrupts — we are in protected mode.

section .text
global _start
extern kernel_main

_start:
    ; Set up the stack pointer
    mov esp, stack_top

    ; Push multiboot parameters for kernel_main(magic, mboot_info)
    ; C calling convention: arguments pushed right-to-left
    push ebx                ; multiboot_info_t* mboot_info
    push eax                ; uint32_t magic

    ; Call the C kernel entry point
    call kernel_main

    ; If kernel_main returns (it shouldn't), halt the CPU
.hang:
    cli                     ; Disable interrupts
    hlt                     ; Halt the processor
    jmp .hang               ; Safety loop in case of NMI
