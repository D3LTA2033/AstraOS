; =============================================================================
; AstraOS - x86 (i686) Boot Entry
; =============================================================================
; Multiboot 1 boot with VESA framebuffer request.
; GRUB sets up a linear framebuffer before handing control to us.
; =============================================================================

section .multiboot
align 4

; ---------------------------------------------------------------------------
; Multiboot Header — with framebuffer request
; ---------------------------------------------------------------------------

MULTIBOOT_MAGIC     equ 0x1BADB002
MULTIBOOT_ALIGN     equ 1 << 0          ; Align modules on page boundaries
MULTIBOOT_MEMINFO   equ 1 << 1          ; Request memory map from GRUB
MULTIBOOT_VIDEO     equ 1 << 2          ; Request video mode info
MULTIBOOT_FLAGS     equ MULTIBOOT_ALIGN | MULTIBOOT_MEMINFO | MULTIBOOT_VIDEO
MULTIBOOT_CHECKSUM  equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

dd MULTIBOOT_MAGIC
dd MULTIBOOT_FLAGS
dd MULTIBOOT_CHECKSUM

; Address fields (unused with ELF, set to 0)
dd 0    ; header_addr
dd 0    ; load_addr
dd 0    ; load_end_addr
dd 0    ; bss_end_addr
dd 0    ; entry_addr

; Video mode request
dd 0    ; mode_type: 0 = linear graphics
dd 1024 ; width
dd 768  ; height
dd 32   ; depth (32 bpp)

; ---------------------------------------------------------------------------
; Kernel Stack
; ---------------------------------------------------------------------------
section .bss
align 16
stack_bottom:
    resb 16384              ; 16 KB stack
stack_top:

; ---------------------------------------------------------------------------
; Entry Point
; ---------------------------------------------------------------------------
section .text
global _start
extern kernel_main

_start:
    mov esp, stack_top

    ; Push multiboot parameters for kernel_main(magic, mboot_info)
    push ebx                ; multiboot_info_t* mboot_info
    push eax                ; uint32_t magic

    call kernel_main

.hang:
    cli
    hlt
    jmp .hang
