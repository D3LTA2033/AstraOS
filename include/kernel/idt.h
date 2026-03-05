/* ==========================================================================
 * AstraOS - Interrupt Descriptor Table (IDT)
 * ==========================================================================
 * The IDT maps interrupt/exception numbers (0-255) to handler routines.
 *
 * Vectors 0-31:   CPU exceptions (divide error, page fault, GPF, etc.)
 * Vectors 32-47:  Hardware IRQs (remapped from PIC defaults)
 * Vectors 48-255: Available for software interrupts (syscalls, etc.)
 * ========================================================================== */

#ifndef _ASTRA_KERNEL_IDT_H
#define _ASTRA_KERNEL_IDT_H

#include <kernel/types.h>

#define IDT_ENTRIES 256

/* IDT gate types */
#define IDT_GATE_INT32  0x8E    /* P=1, DPL=0, 32-bit interrupt gate */
#define IDT_GATE_TRAP32 0x8F    /* P=1, DPL=0, 32-bit trap gate */
#define IDT_GATE_USER   0xEE    /* P=1, DPL=3, 32-bit interrupt gate (callable from ring 3) */

/* IDT entry (8 bytes) */
typedef struct __attribute__((packed)) {
    uint16_t base_low;      /* Handler address bits 0-15 */
    uint16_t selector;      /* Kernel code segment selector */
    uint8_t  zero;          /* Always 0 */
    uint8_t  flags;         /* Type and attributes */
    uint16_t base_high;     /* Handler address bits 16-31 */
} idt_entry_t;

/* IDT pointer — loaded by LIDT instruction */
typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint32_t base;
} idt_ptr_t;

/* CPU register state pushed by ISR stubs */
typedef struct __attribute__((packed)) {
    /* Pushed by our stub: pusha equivalent */
    uint32_t ds;
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;

    /* Pushed by our stub */
    uint32_t int_no;
    uint32_t err_code;

    /* Pushed by CPU automatically */
    uint32_t eip, cs, eflags, useresp, ss;
} registers_t;

/* Interrupt handler function signature */
typedef void (*isr_handler_t)(registers_t *regs);

/* Initialize the IDT */
void idt_init(void);

/* Set a specific IDT gate */
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);

/* Register a handler for an interrupt vector */
void isr_register_handler(uint8_t n, isr_handler_t handler);

/* Assembly routine to load IDT */
extern void idt_flush(uint32_t idt_ptr);

/* ---- ISR stubs (CPU exceptions 0-31) ---- */
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

/* ---- IRQ stubs (hardware interrupts 0-15 -> vectors 32-47) ---- */
extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

/* Software interrupt stubs */
extern void isr128(void);  /* int 0x80 — syscall */

#endif /* _ASTRA_KERNEL_IDT_H */
