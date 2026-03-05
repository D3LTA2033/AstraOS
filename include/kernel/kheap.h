/* ==========================================================================
 * AstraOS - Kernel Heap Allocator
 * ==========================================================================
 * First-fit free-list allocator for kernel dynamic memory.
 *
 * Design:
 *   - Each allocation has an 8-byte header: [size | is_free]
 *   - Free blocks are coalesced on free() to reduce fragmentation
 *   - The heap grows by mapping new pages via VMM when exhausted
 *   - Allocations are 8-byte aligned for performance
 *
 * This is intentionally simple. A slab allocator or buddy system can
 * replace it in later phases if fragmentation becomes an issue.
 * ========================================================================== */

#ifndef _ASTRA_KERNEL_KHEAP_H
#define _ASTRA_KERNEL_KHEAP_H

#include <kernel/types.h>

/* Alignment for all allocations */
#define KHEAP_ALIGN 8

/* Initialize the kernel heap */
void kheap_init(void);

/* Allocate size bytes from kernel heap */
void *kmalloc(size_t size);

/* Allocate and zero-initialize */
void *kcalloc(size_t count, size_t size);

/* Free a previously allocated block */
void kfree(void *ptr);

/* Get heap statistics */
void kheap_stats(size_t *total, size_t *used, size_t *free_bytes);

#endif /* _ASTRA_KERNEL_KHEAP_H */
