/* ==========================================================================
 * AstraOS - GDT Implementation
 * ==========================================================================
 * Sets up a flat memory model with 5 segments + TSS:
 *   0x00: Null descriptor (required by CPU)
 *   0x08: Kernel code (ring 0, execute/read)
 *   0x10: Kernel data (ring 0, read/write)
 *   0x18: User code   (ring 3, execute/read)
 *   0x20: User data   (ring 3, read/write)
 *   0x28: TSS         (for ring transitions)
 * ========================================================================== */

#include <kernel/gdt.h>
#include <kernel/kstring.h>

static gdt_entry_t gdt_entries[GDT_NUM_ENTRIES];
static gdt_ptr_t   gdt_ptr;
static tss_entry_t tss;

/* Encode a GDT entry from human-readable parameters */
static void gdt_set_gate(int idx, uint32_t base, uint32_t limit,
                         uint8_t access, uint8_t gran)
{
    gdt_entries[idx].base_low    = (uint16_t)(base & 0xFFFF);
    gdt_entries[idx].base_middle = (uint8_t)((base >> 16) & 0xFF);
    gdt_entries[idx].base_high   = (uint8_t)((base >> 24) & 0xFF);

    gdt_entries[idx].limit_low   = (uint16_t)(limit & 0xFFFF);
    gdt_entries[idx].granularity = (uint8_t)((limit >> 16) & 0x0F);
    gdt_entries[idx].granularity |= (gran & 0xF0);

    gdt_entries[idx].access = access;
}

static void tss_write(int idx, uint16_t ss0, uint32_t esp0)
{
    uint32_t base  = (uint32_t)&tss;
    uint32_t limit = base + sizeof(tss_entry_t);

    /* TSS descriptor: present, ring 0, system segment, type = 0x9 (available TSS) */
    gdt_set_gate(idx, base, limit, 0xE9, 0x00);

    kmemset(&tss, 0, sizeof(tss_entry_t));
    tss.ss0  = ss0;
    tss.esp0 = esp0;

    /* Set segment selectors to kernel data segment for the TSS */
    tss.cs = GDT_KERNEL_CODE | 0x3;  /* Ring 3 can use kernel code via call gate */
    tss.ss = tss.ds = tss.es = tss.fs = tss.gs = GDT_KERNEL_DATA | 0x3;
}

void tss_set_kernel_stack(uint32_t stack)
{
    tss.esp0 = stack;
}

void gdt_init(void)
{
    gdt_ptr.limit = (sizeof(gdt_entry_t) * GDT_NUM_ENTRIES) - 1;
    gdt_ptr.base  = (uint32_t)&gdt_entries;

    /* Access byte format:
     * Bit 7: Present (1)
     * Bit 6-5: DPL (privilege level, 0=kernel, 3=user)
     * Bit 4: Descriptor type (1=code/data, 0=system)
     * Bit 3: Executable (1=code, 0=data)
     * Bit 2: Direction/Conforming
     * Bit 1: Read/Write
     * Bit 0: Accessed
     *
     * Granularity byte:
     * Bit 7: Granularity (1=4KB pages, limit = limit*4096)
     * Bit 6: Size (1=32-bit, 0=16-bit)
     * Bit 5-4: Reserved
     * Bit 3-0: Limit bits 16-19
     */

    /*          idx  base  limit       access  gran  */
    gdt_set_gate(0, 0, 0x00000000, 0x00, 0x00);  /* Null */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);  /* Kernel code: P=1 DPL=0 S=1 E=1 RW=1 */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);  /* Kernel data: P=1 DPL=0 S=1 E=0 RW=1 */
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);  /* User code:   P=1 DPL=3 S=1 E=1 RW=1 */
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);  /* User data:   P=1 DPL=3 S=1 E=0 RW=1 */

    /* TSS: kernel stack = 0 for now (updated when scheduler starts) */
    tss_write(5, GDT_KERNEL_DATA, 0x0);

    gdt_flush((uint32_t)&gdt_ptr);
    tss_flush();
}
