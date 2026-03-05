; =============================================================================
; AstraOS - GDT/TSS Flush Routines
; =============================================================================
; After modifying the GDT in memory, we must reload the GDTR register and
; perform a far jump to reload CS with the new code segment selector.
; All other segment registers are reloaded with the kernel data selector.
; =============================================================================

section .text

; void gdt_flush(uint32_t gdt_ptr_addr)
global gdt_flush
gdt_flush:
    mov eax, [esp + 4]     ; Get pointer to GDT descriptor
    lgdt [eax]              ; Load GDT register

    ; Reload segment registers with kernel data segment (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Far jump to reload CS with kernel code segment (0x08)
    ; This is the only way to load CS in protected mode
    jmp 0x08:.flush_done

.flush_done:
    ret

; void tss_flush(void)
; Load the TSS selector (0x28) into the task register
global tss_flush
tss_flush:
    mov ax, 0x28            ; TSS segment selector
    ltr ax                  ; Load task register
    ret
