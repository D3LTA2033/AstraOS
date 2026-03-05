/* ==========================================================================
 * AstraOS - RAM Filesystem (ramfs) Implementation
 * ==========================================================================
 * A simple in-memory filesystem where file data is stored in kernel heap
 * allocations. Supports creating files with initial content, reading, and
 * writing. Used to populate /etc and other early-boot paths before a
 * real disk filesystem is available.
 *
 * Each ramfs file stores its data in a contiguous heap buffer. The buffer
 * grows on writes past the current size. There is no block abstraction —
 * this is intentionally simple for Phase 6.
 * ========================================================================== */

#include <kernel/vfs.h>
#include <kernel/kheap.h>
#include <kernel/kstring.h>

/* Per-file backing store */
typedef struct {
    uint8_t  *data;         /* Heap-allocated file content */
    uint32_t  capacity;     /* Allocated buffer size */
} ramfs_file_t;

/* --- ramfs node operations --- */

static uint32_t ramfs_read(vfs_node_t *node, uint32_t offset,
                           uint32_t size, uint8_t *buffer)
{
    ramfs_file_t *rf = (ramfs_file_t *)node->private_data;
    if (!rf || !rf->data)
        return 0;

    if (offset >= node->size)
        return 0;

    uint32_t avail = node->size - offset;
    if (size > avail)
        size = avail;

    kmemcpy(buffer, rf->data + offset, size);
    return size;
}

static uint32_t ramfs_write(vfs_node_t *node, uint32_t offset,
                            uint32_t size, const uint8_t *buffer)
{
    ramfs_file_t *rf = (ramfs_file_t *)node->private_data;
    if (!rf)
        return 0;

    uint32_t end = offset + size;

    /* Grow buffer if needed */
    if (end > rf->capacity) {
        uint32_t new_cap = rf->capacity;
        if (new_cap == 0)
            new_cap = 256;
        while (new_cap < end)
            new_cap *= 2;

        uint8_t *new_data = (uint8_t *)kmalloc(new_cap);
        if (!new_data)
            return 0;

        if (rf->data && node->size > 0)
            kmemcpy(new_data, rf->data, node->size);

        /* Zero the gap between old size and offset if needed */
        if (offset > node->size)
            kmemset(new_data + node->size, 0, offset - node->size);

        if (rf->data)
            kfree(rf->data);

        rf->data = new_data;
        rf->capacity = new_cap;
    }

    kmemcpy(rf->data + offset, buffer, size);
    if (end > node->size)
        node->size = end;

    return size;
}

/* --- Public API --- */

vfs_node_t *ramfs_create_file(const char *name, const void *data,
                              uint32_t size)
{
    vfs_node_t *node = vfs_create_node(name, VFS_FILE);
    if (!node)
        return NULL;

    ramfs_file_t *rf = (ramfs_file_t *)kcalloc(1, sizeof(ramfs_file_t));
    if (!rf) {
        kfree(node);
        return NULL;
    }

    node->read  = ramfs_read;
    node->write = ramfs_write;
    node->private_data = rf;

    if (data && size > 0) {
        rf->capacity = size;
        rf->data = (uint8_t *)kmalloc(size);
        if (rf->data) {
            kmemcpy(rf->data, data, size);
            node->size = size;
        }
    }

    return node;
}

vfs_node_t *ramfs_create_dir(const char *name)
{
    return vfs_create_node(name, VFS_DIRECTORY);
}
