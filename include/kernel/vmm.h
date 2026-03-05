/* ==========================================================================
 * AstraOS - Virtual Memory Manager (VMM)
 * ==========================================================================
 * Manages the x86 two-level page table structure:
 *   - Page Directory: 1024 entries, each pointing to a Page Table
 *   - Page Table: 1024 entries, each mapping a 4KB page
 *   - Total addressable: 1024 * 1024 * 4KB = 4GB
 *
 * Memory layout (virtual):
 *   0x00000000 - 0x003FFFFF  Identity-mapped low memory (kernel + boot data)
 *   0x00400000 - 0xBFFFFFFF  Available for user space (Phase 5)
 *   0xC0000000 - 0xFEFFFFFF  Kernel heap and mappings
 *   0xFF000000 - 0xFFFFFFFF  Reserved (recursive page table mapping future)
 *
 * For Phase 3, the kernel is identity-mapped (virtual == physical).
 * Higher-half kernel relocation can be done in a later phase.
 * ========================================================================== */

#ifndef _ASTRA_KERNEL_VMM_H
#define _ASTRA_KERNEL_VMM_H

#include <kernel/types.h>

/* Page size */
#define PAGE_SIZE       4096
#define PAGE_ENTRIES    1024

/* Page flags (bits in page directory/table entries) */
#define PAGE_PRESENT    0x001   /* Page is present in memory */
#define PAGE_WRITABLE   0x002   /* Page is writable */
#define PAGE_USER       0x004   /* Page accessible from ring 3 */
#define PAGE_WRITETHROUGH 0x008
#define PAGE_NOCACHE    0x010
#define PAGE_ACCESSED   0x020
#define PAGE_DIRTY      0x040
#define PAGE_SIZE_4MB   0x080   /* Page directory: 4MB page (not used) */

/* Common flag combinations */
#define PAGE_KERNEL     (PAGE_PRESENT | PAGE_WRITABLE)
#define PAGE_USERMODE   (PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER)

/* Kernel heap virtual address range */
#define KHEAP_START     0xC0000000
#define KHEAP_INITIAL   (1024 * 1024)       /* 1MB initial heap */
#define KHEAP_MAX       (256 * 1024 * 1024) /* 256MB max heap */

/* Page directory entry (PDE) and page table entry (PTE) are both uint32_t */
typedef uint32_t page_entry_t;

/* Page table — 1024 entries mapping 4MB of virtual address space */
typedef struct __attribute__((aligned(PAGE_SIZE))) {
    page_entry_t entries[PAGE_ENTRIES];
} page_table_t;

/* Page directory — 1024 entries, one per page table */
typedef struct __attribute__((aligned(PAGE_SIZE))) {
    page_entry_t entries[PAGE_ENTRIES];
    /* Physical addresses of page tables (for when paging is active) */
    page_table_t *tables[PAGE_ENTRIES];
} page_directory_t;

/* Initialize virtual memory — identity map kernel, enable paging */
void vmm_init(void);

/* Map a virtual address to a physical address with given flags */
void vmm_map_page(page_directory_t *dir, uint32_t virt, uint32_t phys, uint32_t flags);

/* Unmap a virtual page */
void vmm_unmap_page(page_directory_t *dir, uint32_t virt);

/* Get the physical address mapped to a virtual address. Returns 0 if unmapped. */
uint32_t vmm_get_physical(page_directory_t *dir, uint32_t virt);

/* Get the current kernel page directory */
page_directory_t *vmm_get_kernel_directory(void);

/* Assembly helpers */
extern void paging_load_directory(uint32_t phys_addr);
extern void paging_enable(void);
extern void paging_disable(void);
extern uint32_t paging_get_cr2(void);
extern void paging_flush_tlb(uint32_t virt_addr);

#endif /* _ASTRA_KERNEL_VMM_H */
