; =============================================================================
; AstraOS - Interrupt Service Routine (ISR) and IRQ Assembly Stubs
; =============================================================================
; These stubs create a uniform stack frame for the C handler regardless of
; whether the CPU pushes an error code. The common stub saves all registers,
; sets up kernel data segments, calls the C handler, then restores state.
;
; Stack frame on entry to isr_common_stub:
;   [SS, ESP, EFLAGS, CS, EIP]    <- pushed by CPU
;   [error_code]                   <- pushed by CPU (or 0 by our stub)
;   [int_no]                       <- pushed by our stub
;   [EAX..EDI]                     <- pushed by pusha
;   [DS]                           <- pushed by our stub
; =============================================================================

section .text

; --- IDT flush ---
global idt_flush
idt_flush:
    mov eax, [esp + 4]
    lidt [eax]
    ret

; --- Common ISR handler stub ---
extern isr_handler

isr_common_stub:
    pusha                   ; Push EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI

    mov ax, ds
    push eax                ; Save data segment

    ; Load kernel data segment
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Pass pointer to register struct (stack pointer)
    push esp
    call isr_handler
    add esp, 4              ; Clean up pushed argument

    pop eax                 ; Restore original data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa                    ; Restore general-purpose registers
    add esp, 8              ; Remove int_no and error_code from stack
    iret                    ; Return from interrupt (pops CS, EIP, EFLAGS, [SS, ESP])

; =============================================================================
; ISR stubs for CPU exceptions (0-31)
; Some exceptions push an error code, some don't. For those that don't,
; we push a dummy 0 to keep the stack frame consistent.
; =============================================================================

; Macro for ISR without error code (CPU doesn't push one)
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push dword 0            ; Dummy error code
    push dword %1           ; Interrupt number
    jmp isr_common_stub
%endmacro

; Macro for ISR with error code (CPU pushes it automatically)
%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push dword %1           ; Interrupt number (error code already on stack)
    jmp isr_common_stub
%endmacro

; CPU exceptions 0-31
ISR_NOERRCODE 0             ; Divide by zero
ISR_NOERRCODE 1             ; Debug
ISR_NOERRCODE 2             ; NMI
ISR_NOERRCODE 3             ; Breakpoint
ISR_NOERRCODE 4             ; Overflow
ISR_NOERRCODE 5             ; Bound range exceeded
ISR_NOERRCODE 6             ; Invalid opcode
ISR_NOERRCODE 7             ; Device not available
ISR_ERRCODE   8             ; Double fault (pushes error code)
ISR_NOERRCODE 9             ; Coprocessor segment overrun
ISR_ERRCODE   10            ; Invalid TSS
ISR_ERRCODE   11            ; Segment not present
ISR_ERRCODE   12            ; Stack-segment fault
ISR_ERRCODE   13            ; General protection fault
ISR_ERRCODE   14            ; Page fault
ISR_NOERRCODE 15            ; Reserved
ISR_NOERRCODE 16            ; x87 FPU error
ISR_ERRCODE   17            ; Alignment check
ISR_NOERRCODE 18            ; Machine check
ISR_NOERRCODE 19            ; SIMD floating-point
ISR_NOERRCODE 20            ; Virtualization
ISR_ERRCODE   21            ; Control protection
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_ERRCODE   30            ; Security exception
ISR_NOERRCODE 31

; =============================================================================
; IRQ stubs (hardware interrupts 0-15 -> vectors 32-47)
; IRQs never have error codes from the CPU.
; =============================================================================

%macro IRQ 2
global irq%1
irq%1:
    push dword 0            ; Dummy error code
    push dword %2           ; Interrupt vector number
    jmp isr_common_stub
%endmacro

IRQ 0,  32                  ; PIT timer
IRQ 1,  33                  ; Keyboard
IRQ 2,  34                  ; Cascade (PIC2)
IRQ 3,  35                  ; COM2
IRQ 4,  36                  ; COM1
IRQ 5,  37                  ; LPT2
IRQ 6,  38                  ; Floppy disk
IRQ 7,  39                  ; LPT1 / spurious
IRQ 8,  40                  ; CMOS RTC
IRQ 9,  41                  ; Free / ACPI
IRQ 10, 42                  ; Free
IRQ 11, 43                  ; Free
IRQ 12, 44                  ; PS/2 Mouse
IRQ 13, 45                  ; FPU
IRQ 14, 46                  ; Primary ATA
IRQ 15, 47                  ; Secondary ATA

; =============================================================================
; Software interrupt stubs (syscalls, etc.)
; =============================================================================

; int 0x80 — System call entry point
global isr128
isr128:
    push dword 0            ; Dummy error code
    push dword 128          ; Interrupt vector number
    jmp isr_common_stub
