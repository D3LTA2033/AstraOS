/* ==========================================================================
 * AstraOS - Initramfs (Initial RAM Filesystem)
 * ==========================================================================
 * Parses a USTAR tar archive loaded as a GRUB multiboot module and
 * populates the VFS tree with the contained files and directories.
 * ========================================================================== */

#ifndef _ASTRA_KERNEL_INITRAMFS_H
#define _ASTRA_KERNEL_INITRAMFS_H

#include <kernel/types.h>

/* Load an initramfs tar archive into the VFS.
 * base: physical/virtual address of the tar archive
 * size: size in bytes
 * Returns number of files loaded, or -1 on error. */
int initramfs_load(void *base, uint32_t size);

#endif /* _ASTRA_KERNEL_INITRAMFS_H */
