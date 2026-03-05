/* ==========================================================================
 * AstraOS - Virtual Memory Manager Implementation
 * ==========================================================================
 * Sets up x86 two-level paging:
 *   1. Creates a kernel page directory
 *   2. Identity-maps the first 4MB (covers kernel + VGA + BIOS area)
 *   3. Identity-maps additional memory as needed for the PMM bitmap
 *   4. Pre-maps the kernel heap region
 *   5. Enables paging via CR0
 *
 * After paging is enabled, all memory access goes through the MMU.
 * Identity mapping means virtual == physical for the kernel region,
 * so existing pointers (VGA buffer, PMM bitmap, etc.) continue to work.
 * ========================================================================== */

#include <kernel/vmm.h>
#include <kernel/pmm.h>
#include <kernel/kstring.h>
#include <drivers/vga.h>

/* Linker symbol */
extern uint32_t _kernel_end;

/* The kernel's page directory — must be page-aligned */
static page_directory_t kernel_directory __attribute__((aligned(PAGE_SIZE)));

/* Page tables for the identity-mapped region (first 4MB needs 1 table) */
/* We allocate enough to cover up to 16MB of identity-mapped space */
#define IDENTITY_MAP_TABLES 4
static page_table_t identity_tables[IDENTITY_MAP_TABLES] __attribute__((aligned(PAGE_SIZE)));

/* Track how much of the kernel heap has been mapped */
static uint32_t kheap_mapped_end = KHEAP_START;

/* --- Internal helpers --- */

static inline uint32_t pd_index(uint32_t virt)
{
    return (virt >> 22) & 0x3FF;
}

static inline uint32_t pt_index(uint32_t virt)
{
    return (virt >> 12) & 0x3FF;
}

static inline uint32_t page_align_up(uint32_t addr)
{
    return (addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

/* Hex print helper */
static void print_hex(uint32_t val)
{
    const char hex[] = "0123456789ABCDEF";
    char buf[11] = "0x00000000";
    for (int i = 9; i >= 2; i--) {
        buf[i] = hex[val & 0xF];
        val >>= 4;
    }
    vga_write(buf);
}

static void print_dec(uint32_t val)
{
    if (val == 0) { vga_putchar('0'); return; }
    char buf[12];
    int i = 0;
    while (val > 0) { buf[i++] = '0' + (char)(val % 10); val /= 10; }
    for (int j = i - 1; j >= 0; j--) vga_putchar(buf[j]);
}

void vmm_map_page(page_directory_t *dir, uint32_t virt, uint32_t phys, uint32_t flags)
{
    uint32_t pdi = pd_index(virt);
    uint32_t pti = pt_index(virt);

    /* Check if page table exists */
    if (!(dir->entries[pdi] & PAGE_PRESENT)) {
        /* Allocate a new page table from PMM */
        uint32_t table_phys = pmm_alloc_frame();
        if (!table_phys) {
            vga_set_color(vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
            vga_write("[FAIL] VMM: out of physical memory for page table\n");
            return;
        }

        page_table_t *table = (page_table_t *)table_phys;
        kmemset(table, 0, sizeof(page_table_t));

        dir->tables[pdi] = table;
        dir->entries[pdi] = table_phys | PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER);
    }

    page_table_t *table = dir->tables[pdi];
    table->entries[pti] = (phys & 0xFFFFF000) | (flags & 0xFFF);

    paging_flush_tlb(virt);
}

void vmm_unmap_page(page_directory_t *dir, uint32_t virt)
{
    uint32_t pdi = pd_index(virt);
    uint32_t pti = pt_index(virt);

    if (!(dir->entries[pdi] & PAGE_PRESENT))
        return;

    page_table_t *table = dir->tables[pdi];
    table->entries[pti] = 0;

    paging_flush_tlb(virt);
}

uint32_t vmm_get_physical(page_directory_t *dir, uint32_t virt)
{
    uint32_t pdi = pd_index(virt);
    uint32_t pti = pt_index(virt);

    if (!(dir->entries[pdi] & PAGE_PRESENT))
        return 0;

    page_table_t *table = dir->tables[pdi];
    if (!(table->entries[pti] & PAGE_PRESENT))
        return 0;

    return (table->entries[pti] & 0xFFFFF000) | (virt & 0xFFF);
}

page_directory_t *vmm_get_kernel_directory(void)
{
    return &kernel_directory;
}

void vmm_init(void)
{
    /* Clear the page directory */
    kmemset(&kernel_directory, 0, sizeof(page_directory_t));

    /* Calculate how much physical memory to identity map.
     * We need to cover: low memory (0-1MB), kernel, PMM bitmap.
     * Round up to 4MB boundary (one page table covers 4MB). */
    uint32_t kernel_end_aligned = page_align_up((uint32_t)&_kernel_end);
    /* PMM bitmap sits after kernel, add generous padding */
    uint32_t identity_end = kernel_end_aligned + (256 * 1024); /* +256KB for bitmap */
    /* Round up to next 4MB boundary */
    identity_end = (identity_end + (4 * 1024 * 1024 - 1)) & ~(4 * 1024 * 1024 - 1);
    if (identity_end > IDENTITY_MAP_TABLES * 4 * 1024 * 1024)
        identity_end = IDENTITY_MAP_TABLES * 4 * 1024 * 1024;

    uint32_t num_tables = identity_end / (4 * 1024 * 1024);

    /* Identity map: virtual address == physical address */
    for (uint32_t t = 0; t < num_tables; t++) {
        kmemset(&identity_tables[t], 0, sizeof(page_table_t));

        for (uint32_t i = 0; i < PAGE_ENTRIES; i++) {
            uint32_t phys = (t * PAGE_ENTRIES + i) * PAGE_SIZE;
            identity_tables[t].entries[i] = phys | PAGE_KERNEL;
        }

        kernel_directory.tables[t] = &identity_tables[t];
        kernel_directory.entries[t] = (uint32_t)&identity_tables[t] | PAGE_KERNEL;
    }

    /* Load page directory and enable paging */
    paging_load_directory((uint32_t)&kernel_directory);
    paging_enable();

    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    vga_write("[OK] Paging enabled: ");
    print_dec(num_tables * 4);
    vga_write(" MB identity-mapped\n");

    /* Pre-map initial kernel heap region */
    uint32_t heap_end = KHEAP_START + KHEAP_INITIAL;
    for (uint32_t addr = KHEAP_START; addr < heap_end; addr += PAGE_SIZE) {
        uint32_t frame = pmm_alloc_frame();
        if (!frame) {
            vga_set_color(vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
            vga_write("[FAIL] VMM: cannot allocate heap frames\n");
            return;
        }
        vmm_map_page(&kernel_directory, addr, frame, PAGE_KERNEL);
    }
    kheap_mapped_end = heap_end;

    vga_write("[OK] Kernel heap mapped: ");
    print_hex(KHEAP_START);
    vga_write(" - ");
    print_hex(heap_end - 1);
    vga_write(" (");
    print_dec(KHEAP_INITIAL / 1024);
    vga_write(" KB)\n");
}
