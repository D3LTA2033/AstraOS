; =============================================================================
; AstraOS - User Mode Entry (Ring 0 -> Ring 3)
; =============================================================================
; void enter_usermode(uint32_t eip, uint32_t esp)
;
; Transitions from kernel mode to user mode by crafting a fake IRET frame:
;   [SS]        User data segment (0x20 | 3)
;   [ESP]       User stack pointer
;   [EFLAGS]    With IF=1 (interrupts enabled)
;   [CS]        User code segment (0x18 | 3)
;   [EIP]       User entry point
;
; The RPL (bits 0-1) of segment selectors must be 3 for ring 3.
; =============================================================================

section .text
global enter_usermode

enter_usermode:
    mov ebx, [esp + 4]      ; EIP (user entry point)
    mov ecx, [esp + 8]      ; ESP (user stack)

    ; Set data segments to user data selector (0x20 | 3 = 0x23)
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Build IRET frame
    push dword 0x23          ; SS = user data
    push ecx                 ; ESP = user stack
    pushf
    pop eax
    or eax, 0x200            ; Set IF (interrupt flag)
    push eax                 ; EFLAGS
    push dword 0x1B          ; CS = user code (0x18 | 3)
    push ebx                 ; EIP = user entry

    iret                     ; Jump to ring 3
