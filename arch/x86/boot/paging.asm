; =============================================================================
; AstraOS - Paging Assembly Helpers
; =============================================================================
; Low-level routines for manipulating paging control registers.
; These cannot be done from C — they require direct register access.
; =============================================================================

section .text

; void paging_load_directory(uint32_t phys_addr)
; Load a page directory physical address into CR3
global paging_load_directory
paging_load_directory:
    mov eax, [esp + 4]
    mov cr3, eax
    ret

; void paging_enable(void)
; Set the PG bit (bit 31) in CR0 to enable paging
global paging_enable
paging_enable:
    mov eax, cr0
    or eax, 0x80000000      ; Set PG bit
    mov cr0, eax
    ret

; void paging_disable(void)
; Clear the PG bit in CR0 to disable paging
global paging_disable
paging_disable:
    mov eax, cr0
    and eax, 0x7FFFFFFF     ; Clear PG bit
    mov cr0, eax
    ret

; uint32_t paging_get_cr2(void)
; Read CR2 (faulting linear address on page fault)
global paging_get_cr2
paging_get_cr2:
    mov eax, cr2
    ret

; void paging_flush_tlb(uint32_t virt_addr)
; Invalidate a single TLB entry for the given virtual address
global paging_flush_tlb
paging_flush_tlb:
    mov eax, [esp + 4]
    invlpg [eax]
    ret
