/* ==========================================================================
 * AstraOS - Kernel Heap Allocator Implementation
 * ==========================================================================
 * First-fit free-list allocator.
 *
 * Memory layout of an allocation:
 *   [block_header_t][....user data....]
 *
 * Free blocks form a singly-linked list. On kmalloc, we walk the list
 * looking for the first block large enough. If a block is much larger
 * than needed, we split it. On kfree, we coalesce adjacent free blocks
 * to combat fragmentation.
 * ========================================================================== */

#include <kernel/kheap.h>
#include <kernel/vmm.h>
#include <kernel/pmm.h>
#include <kernel/kstring.h>
#include <drivers/vga.h>

/* Block header — prepended to every allocation */
typedef struct block_header {
    size_t size;                /* Size of user data (not including header) */
    bool   is_free;
    struct block_header *next;
} block_header_t;

#define HEADER_SIZE     sizeof(block_header_t)
/* Minimum block size to avoid excessive fragmentation from splits */
#define MIN_BLOCK_SIZE  16

static block_header_t *free_list = NULL;
static uint32_t heap_start;
static uint32_t heap_end;
static uint32_t heap_max;

/* Align a value up to KHEAP_ALIGN boundary */
static inline size_t align_up(size_t val)
{
    return (val + KHEAP_ALIGN - 1) & ~(KHEAP_ALIGN - 1);
}

/* Grow the heap by mapping new pages */
static bool heap_grow(size_t min_bytes)
{
    size_t grow = min_bytes;
    /* Grow in PAGE_SIZE increments, at least 16 pages at a time */
    if (grow < 16 * PAGE_SIZE)
        grow = 16 * PAGE_SIZE;
    grow = (grow + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    if (heap_end + grow > heap_max)
        return false;

    page_directory_t *dir = vmm_get_kernel_directory();

    for (uint32_t addr = heap_end; addr < heap_end + grow; addr += PAGE_SIZE) {
        uint32_t frame = pmm_alloc_frame();
        if (!frame)
            return false;
        vmm_map_page(dir, addr, frame, PAGE_KERNEL);
    }

    /* Create a free block covering the new region */
    block_header_t *new_block = (block_header_t *)heap_end;
    new_block->size    = grow - HEADER_SIZE;
    new_block->is_free = true;
    new_block->next    = NULL;

    /* Append to end of free list */
    if (!free_list) {
        free_list = new_block;
    } else {
        block_header_t *cur = free_list;
        while (cur->next)
            cur = cur->next;

        /* Coalesce with last block if it's free and adjacent */
        if (cur->is_free && (uint32_t)cur + HEADER_SIZE + cur->size == (uint32_t)new_block) {
            cur->size += HEADER_SIZE + new_block->size;
        } else {
            cur->next = new_block;
        }
    }

    heap_end += grow;
    return true;
}

/* Split a block if it's large enough to hold the allocation + a new free block */
static void split_block(block_header_t *block, size_t size)
{
    size_t remaining = block->size - size - HEADER_SIZE;
    if (remaining < MIN_BLOCK_SIZE)
        return;     /* Not enough space for a useful split */

    block_header_t *new_block = (block_header_t *)((uint8_t *)block + HEADER_SIZE + size);
    new_block->size    = remaining;
    new_block->is_free = true;
    new_block->next    = block->next;

    block->size = size;
    block->next = new_block;
}

/* Coalesce adjacent free blocks */
static void coalesce(void)
{
    block_header_t *cur = free_list;
    while (cur && cur->next) {
        if (cur->is_free && cur->next->is_free) {
            /* Merge next block into current */
            cur->size += HEADER_SIZE + cur->next->size;
            cur->next = cur->next->next;
            /* Don't advance — check if we can merge again */
        } else {
            cur = cur->next;
        }
    }
}

void kheap_init(void)
{
    heap_start = KHEAP_START;
    heap_end   = KHEAP_START + KHEAP_INITIAL;
    heap_max   = KHEAP_START + KHEAP_MAX;

    /* The VMM has already mapped KHEAP_START..+KHEAP_INITIAL.
     * Create one large free block spanning the entire initial heap. */
    free_list = (block_header_t *)heap_start;
    free_list->size    = KHEAP_INITIAL - HEADER_SIZE;
    free_list->is_free = true;
    free_list->next    = NULL;
}

void *kmalloc(size_t size)
{
    if (size == 0)
        return NULL;

    size = align_up(size);

    /* First-fit search */
    block_header_t *cur = free_list;
    while (cur) {
        if (cur->is_free && cur->size >= size) {
            split_block(cur, size);
            cur->is_free = false;
            return (void *)((uint8_t *)cur + HEADER_SIZE);
        }
        cur = cur->next;
    }

    /* No suitable block found — grow the heap */
    if (!heap_grow(size + HEADER_SIZE))
        return NULL;    /* Out of memory */

    /* Retry — the new block should be at the end of the list */
    cur = free_list;
    while (cur) {
        if (cur->is_free && cur->size >= size) {
            split_block(cur, size);
            cur->is_free = false;
            return (void *)((uint8_t *)cur + HEADER_SIZE);
        }
        cur = cur->next;
    }

    return NULL;    /* Should not reach here */
}

void *kcalloc(size_t count, size_t size)
{
    size_t total = count * size;
    void *ptr = kmalloc(total);
    if (ptr)
        kmemset(ptr, 0, total);
    return ptr;
}

void kfree(void *ptr)
{
    if (!ptr)
        return;

    block_header_t *block = (block_header_t *)((uint8_t *)ptr - HEADER_SIZE);

    /* Basic sanity check */
    if ((uint32_t)block < heap_start || (uint32_t)block >= heap_end)
        return;     /* Pointer outside heap — ignore silently */

    block->is_free = true;
    coalesce();
}

void kheap_stats(size_t *total, size_t *used, size_t *free_bytes)
{
    size_t t = 0, u = 0, f = 0;
    block_header_t *cur = free_list;
    while (cur) {
        t += cur->size + HEADER_SIZE;
        if (cur->is_free)
            f += cur->size;
        else
            u += cur->size;
        cur = cur->next;
    }
    if (total) *total = t;
    if (used) *used = u;
    if (free_bytes) *free_bytes = f;
}
