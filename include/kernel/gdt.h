/* ==========================================================================
 * AstraOS - Global Descriptor Table (GDT)
 * ==========================================================================
 * The GDT defines memory segments for the CPU's protected mode. In a flat
 * memory model (which we use), all segments span the full 4GB address space.
 * Segmentation is effectively disabled; real memory protection comes from
 * paging (Phase 3).
 *
 * We still need the GDT because:
 *   1. x86 requires it in protected mode
 *   2. Privilege levels (ring 0 vs ring 3) are encoded in segment selectors
 *   3. The TSS segment is required for hardware task switching / stack switching
 * ========================================================================== */

#ifndef _ASTRA_KERNEL_GDT_H
#define _ASTRA_KERNEL_GDT_H

#include <kernel/types.h>

/* Segment selector indices (byte offset = index * 8) */
#define GDT_NULL        0x00
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE   0x18
#define GDT_USER_DATA   0x20
#define GDT_TSS         0x28

#define GDT_NUM_ENTRIES 6

/* GDT entry (8 bytes, packed as the CPU expects) */
typedef struct __attribute__((packed)) {
    uint16_t limit_low;     /* Limit bits 0-15 */
    uint16_t base_low;      /* Base bits 0-15 */
    uint8_t  base_middle;   /* Base bits 16-23 */
    uint8_t  access;        /* Access byte */
    uint8_t  granularity;   /* Flags + limit bits 16-19 */
    uint8_t  base_high;     /* Base bits 24-31 */
} gdt_entry_t;

/* GDT pointer — loaded by LGDT instruction */
typedef struct __attribute__((packed)) {
    uint16_t limit;         /* Size of GDT - 1 */
    uint32_t base;          /* Linear address of GDT */
} gdt_ptr_t;

/* TSS (Task State Segment) — needed for ring transitions */
typedef struct __attribute__((packed)) {
    uint32_t prev_tss;
    uint32_t esp0;          /* Kernel stack pointer (used on ring 3 -> 0) */
    uint32_t ss0;           /* Kernel stack segment */
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} tss_entry_t;

/* Initialize GDT and load it via LGDT + far jump */
void gdt_init(void);

/* Update the kernel stack pointer in the TSS (called during task switch) */
void tss_set_kernel_stack(uint32_t stack);

/* Assembly routine to flush GDT — defined in gdt_flush.asm */
extern void gdt_flush(uint32_t gdt_ptr);

/* Assembly routine to flush TSS — defined in gdt_flush.asm */
extern void tss_flush(void);

#endif /* _ASTRA_KERNEL_GDT_H */
