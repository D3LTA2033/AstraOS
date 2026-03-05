/* ==========================================================================
 * AstraOS - RAM Filesystem Interface
 * ==========================================================================
 * In-memory filesystem backed by kernel heap. Files are created with
 * optional initial content. Supports read and write operations.
 * ========================================================================== */

#ifndef _ASTRA_KERNEL_RAMFS_H
#define _ASTRA_KERNEL_RAMFS_H

#include <kernel/vfs.h>

/* Create a ramfs file node with optional initial data.
 * Pass data=NULL and size=0 for an empty file. */
vfs_node_t *ramfs_create_file(const char *name, const void *data,
                              uint32_t size);

/* Create a ramfs directory node */
vfs_node_t *ramfs_create_dir(const char *name);

#endif /* _ASTRA_KERNEL_RAMFS_H */
