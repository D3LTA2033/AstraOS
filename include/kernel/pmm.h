/* ==========================================================================
 * AstraOS - Physical Memory Manager (PMM)
 * ==========================================================================
 * Bitmap-based physical frame allocator. Each bit represents one 4KB page
 * frame. Bit = 1 means the frame is in use.
 *
 * Design:
 *   - The bitmap is placed right after the kernel in physical memory.
 *   - Frame 0 (address 0x0) is always marked reserved to catch NULL derefs.
 *   - The kernel region and the bitmap itself are marked as used.
 *   - Multiboot memory map is used to identify available RAM.
 *
 * Limitations (Phase 2):
 *   - Single-frame allocation only (no contiguous multi-frame alloc yet).
 *   - No NUMA awareness.
 *   - These will be addressed when needed in later phases.
 * ========================================================================== */

#ifndef _ASTRA_KERNEL_PMM_H
#define _ASTRA_KERNEL_PMM_H

#include <kernel/types.h>
#include <kernel/multiboot.h>

#define PMM_FRAME_SIZE  4096        /* 4KB page frames */
#define PMM_FRAMES_PER_BYTE 8       /* 8 frames per bitmap byte */

/* Initialize PMM from multiboot memory map */
void pmm_init(multiboot_info_t *mboot);

/* Allocate a single physical frame. Returns physical address, or 0 on failure. */
uint32_t pmm_alloc_frame(void);

/* Free a previously allocated physical frame */
void pmm_free_frame(uint32_t phys_addr);

/* Mark a specific frame as used (for reserving memory regions) */
void pmm_mark_used(uint32_t phys_addr);

/* Get total and free frame counts */
uint32_t pmm_total_frames(void);
uint32_t pmm_free_frames(void);

#endif /* _ASTRA_KERNEL_PMM_H */
