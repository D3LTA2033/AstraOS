/* ==========================================================================
 * AstraOS - Physical Memory Manager Implementation
 * ==========================================================================
 * Bitmap allocator. We walk the multiboot memory map to find usable RAM,
 * then mark everything as used and selectively free the available regions.
 * This is safer than the reverse (marking all free, then reserving) because
 * we won't accidentally hand out reserved memory if we miss a region.
 * ========================================================================== */

#include <kernel/pmm.h>
#include <kernel/kstring.h>
#include <drivers/vga.h>

/* Linker-provided symbols for kernel boundaries */
extern uint32_t _kernel_end;

/* Bitmap storage */
static uint8_t *bitmap;
static uint32_t total_frames;
static uint32_t bitmap_size;        /* In bytes */
static uint32_t used_frames;

/* --- Bitmap helpers --- */

static inline void bitmap_set(uint32_t frame)
{
    bitmap[frame / 8] |= (1 << (frame % 8));
}

static inline void bitmap_clear(uint32_t frame)
{
    bitmap[frame / 8] &= ~(1 << (frame % 8));
}

static inline bool bitmap_test(uint32_t frame)
{
    return (bitmap[frame / 8] >> (frame % 8)) & 1;
}

/* Find first free frame (bit = 0). Returns frame index or (uint32_t)-1. */
static uint32_t bitmap_find_free(void)
{
    for (uint32_t i = 0; i < bitmap_size; i++) {
        if (bitmap[i] == 0xFF)
            continue;   /* All 8 frames in this byte are used */

        for (uint8_t bit = 0; bit < 8; bit++) {
            uint32_t frame = i * 8 + bit;
            if (frame >= total_frames)
                return (uint32_t)-1;
            if (!bitmap_test(frame))
                return frame;
        }
    }
    return (uint32_t)-1;
}

/* --- Hex printing helper for boot diagnostics --- */
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
    if (val == 0) {
        vga_putchar('0');
        return;
    }
    char buf[12];
    int i = 0;
    while (val > 0) {
        buf[i++] = '0' + (char)(val % 10);
        val /= 10;
    }
    /* Reverse */
    for (int j = i - 1; j >= 0; j--)
        vga_putchar(buf[j]);
}

void pmm_init(multiboot_info_t *mboot)
{
    if (!(mboot->flags & MULTIBOOT_FLAG_MMAP)) {
        vga_set_color(vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
        vga_write("[FAIL] No memory map from bootloader!\n");
        return;
    }

    /* Determine total physical memory from the memory map */
    uint32_t max_addr = 0;
    multiboot_mmap_entry_t *entry = (multiboot_mmap_entry_t *)mboot->mmap_addr;
    multiboot_mmap_entry_t *end   = (multiboot_mmap_entry_t *)(mboot->mmap_addr + mboot->mmap_length);

    while (entry < end) {
        uint32_t region_end = (uint32_t)(entry->base_addr + entry->length);
        if (entry->type == MULTIBOOT_MMAP_AVAILABLE && region_end > max_addr)
            max_addr = region_end;

        /* Move to next entry — size field + 4 bytes for the size field itself */
        entry = (multiboot_mmap_entry_t *)((uint32_t)entry + entry->size + sizeof(entry->size));
    }

    /* Calculate bitmap parameters */
    total_frames = max_addr / PMM_FRAME_SIZE;
    bitmap_size  = (total_frames + 7) / 8;     /* Round up */

    /* Place bitmap right after kernel (page-aligned) */
    uint32_t kernel_end = ((uint32_t)&_kernel_end + PMM_FRAME_SIZE - 1) & ~(PMM_FRAME_SIZE - 1);
    bitmap = (uint8_t *)kernel_end;

    /* Mark ALL frames as used initially (safe default) */
    kmemset(bitmap, 0xFF, bitmap_size);
    used_frames = total_frames;

    /* Walk the memory map again — free available regions */
    entry = (multiboot_mmap_entry_t *)mboot->mmap_addr;
    while (entry < end) {
        if (entry->type == MULTIBOOT_MMAP_AVAILABLE) {
            uint32_t base = (uint32_t)entry->base_addr;
            uint32_t len  = (uint32_t)entry->length;

            /* Align base up and length down to frame boundaries */
            uint32_t frame_start = (base + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;
            uint32_t frame_end   = (base + len) / PMM_FRAME_SIZE;

            for (uint32_t f = frame_start; f < frame_end && f < total_frames; f++) {
                bitmap_clear(f);
                used_frames--;
            }
        }
        entry = (multiboot_mmap_entry_t *)((uint32_t)entry + entry->size + sizeof(entry->size));
    }

    /* Re-reserve critical regions */

    /* Frame 0: reserved for NULL pointer protection */
    if (!bitmap_test(0)) {
        bitmap_set(0);
        used_frames++;
    }

    /* Kernel image + bitmap region */
    uint32_t reserved_end = kernel_end + bitmap_size;
    uint32_t res_frame_start = 0;
    uint32_t res_frame_end   = (reserved_end + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;

    for (uint32_t f = res_frame_start; f < res_frame_end && f < total_frames; f++) {
        if (!bitmap_test(f)) {
            bitmap_set(f);
            used_frames++;
        }
    }

    /* Print diagnostics */
    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    vga_write("[OK] PMM initialized: ");
    print_dec(pmm_free_frames());
    vga_write("/");
    print_dec(total_frames);
    vga_write(" frames free (");
    print_dec((pmm_free_frames() * 4) / 1024);
    vga_write(" MB)\n");

    vga_write("     Bitmap at ");
    print_hex((uint32_t)bitmap);
    vga_write(", size ");
    print_dec(bitmap_size);
    vga_write(" bytes\n");
}

uint32_t pmm_alloc_frame(void)
{
    uint32_t frame = bitmap_find_free();
    if (frame == (uint32_t)-1)
        return 0;   /* Out of memory */

    bitmap_set(frame);
    used_frames++;
    return frame * PMM_FRAME_SIZE;
}

void pmm_free_frame(uint32_t phys_addr)
{
    uint32_t frame = phys_addr / PMM_FRAME_SIZE;
    if (frame >= total_frames)
        return;

    if (bitmap_test(frame)) {
        bitmap_clear(frame);
        used_frames--;
    }
}

void pmm_mark_used(uint32_t phys_addr)
{
    uint32_t frame = phys_addr / PMM_FRAME_SIZE;
    if (frame >= total_frames)
        return;

    if (!bitmap_test(frame)) {
        bitmap_set(frame);
        used_frames++;
    }
}

uint32_t pmm_total_frames(void)
{
    return total_frames;
}

uint32_t pmm_free_frames(void)
{
    return total_frames - used_frames;
}
