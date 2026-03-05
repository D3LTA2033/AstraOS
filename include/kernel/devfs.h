/* ==========================================================================
 * AstraOS - Device Filesystem (devfs)
 * ==========================================================================
 * Provides /dev as a VFS mount populated with device nodes.
 * Device drivers register character or block devices here, making them
 * accessible through standard open/read/write/close operations.
 *
 * Device nodes are created at registration time and persist for the
 * lifetime of the kernel. Hot-removal is not supported in this phase.
 * ========================================================================== */

#ifndef _ASTRA_KERNEL_DEVFS_H
#define _ASTRA_KERNEL_DEVFS_H

#include <kernel/vfs.h>

/* Initialize devfs and mount it at /dev */
void devfs_init(void);

/* Register a character device under /dev/<name> */
int devfs_register_chardev(const char *name,
                           read_fn_t  read_op,
                           write_fn_t write_op,
                           open_fn_t  open_op,
                           close_fn_t close_op);

#endif /* _ASTRA_KERNEL_DEVFS_H */
