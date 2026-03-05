; =============================================================================
; AstraOS - Context Switch
; =============================================================================
; void context_switch(uint32_t *old_esp, uint32_t new_esp)
;
; Saves the current task's callee-saved registers and ESP onto the current
; stack, stores the stack pointer via *old_esp, then loads new_esp and
; restores the new task's registers.
;
; This follows the cdecl calling convention. The caller-saved registers
; (EAX, ECX, EDX) are already saved by the caller (the C compiler handles
; this). We only need to save EBX, ESI, EDI, EBP, and EFLAGS.
;
; Stack layout after saving (top = lower address):
;   [EFLAGS]
;   [EDI]
;   [ESI]
;   [EBX]
;   [EBP]      <- ESP points here
;
; When we load the new task's ESP and pop, we restore its registers and
; the `ret` returns to wherever that task was when it last switched out.
; =============================================================================

section .text
global context_switch

context_switch:
    ; --- Save current task's context ---
    mov eax, [esp + 4]      ; eax = &old_esp (pointer to save location)
    mov edx, [esp + 8]      ; edx = new_esp

    ; Save callee-saved registers
    pushf                    ; EFLAGS
    push edi
    push esi
    push ebx
    push ebp

    ; Store current ESP into *old_esp
    mov [eax], esp

    ; --- Load new task's context ---
    mov esp, edx

    ; Restore callee-saved registers
    pop ebp
    pop ebx
    pop esi
    pop edi
    popf                     ; EFLAGS

    ; Return to the new task's saved return address (on its stack)
    ret
