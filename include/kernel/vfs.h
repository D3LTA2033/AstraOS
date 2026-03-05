/* ==========================================================================
 * AstraOS - Virtual File System (VFS) Interface
 * ==========================================================================
 * The VFS provides a uniform interface over multiple filesystem
 * implementations. Every file, directory, and device in the system is
 * represented by a vfs_node_t. Filesystem-specific behavior is dispatched
 * through function pointers stored in the node.
 *
 * Design:
 *   - vfs_node_t is the universal inode. It contains metadata (name, type,
 *     size, permissions) and a set of operation function pointers.
 *   - Filesystems register a mount callback and populate nodes.
 *   - Path resolution walks the VFS tree from the root node.
 *   - Device files (VGA console, serial port) are nodes with custom
 *     read/write ops, enabling uniform I/O through open/read/write/close.
 *
 * The VFS does NOT own the backing storage — that is the filesystem's job.
 * ramfs stores data in kernel heap; future disk-backed FSes will use
 * block device I/O.
 * ========================================================================== */

#ifndef _ASTRA_KERNEL_VFS_H
#define _ASTRA_KERNEL_VFS_H

#include <kernel/types.h>

/* Maximum filename length (excluding null terminator) */
#define VFS_NAME_MAX    63

/* Maximum path length */
#define VFS_PATH_MAX    256

/* Maximum children per directory node */
#define VFS_MAX_CHILDREN 32

/* Node types */
#define VFS_FILE        0x01
#define VFS_DIRECTORY   0x02
#define VFS_CHARDEVICE  0x04
#define VFS_BLOCKDEVICE 0x08
#define VFS_PIPE        0x10
#define VFS_SYMLINK     0x20
#define VFS_MOUNTPOINT  0x40

/* Forward declaration */
struct vfs_node;

/* File operation function signatures */
typedef uint32_t (*read_fn_t)(struct vfs_node *node, uint32_t offset,
                              uint32_t size, uint8_t *buffer);
typedef uint32_t (*write_fn_t)(struct vfs_node *node, uint32_t offset,
                               uint32_t size, const uint8_t *buffer);
typedef int      (*open_fn_t)(struct vfs_node *node, uint32_t flags);
typedef int      (*close_fn_t)(struct vfs_node *node);
typedef struct vfs_node *(*finddir_fn_t)(struct vfs_node *dir,
                                         const char *name);

/* VFS node — the universal file/directory/device representation */
typedef struct vfs_node {
    char        name[VFS_NAME_MAX + 1]; /* Null-terminated filename */
    uint32_t    type;                   /* VFS_FILE, VFS_DIRECTORY, etc. */
    uint32_t    permissions;            /* rwx bits (future use) */
    uint32_t    uid;                    /* Owner user ID (future use) */
    uint32_t    gid;                    /* Owner group ID (future use) */
    uint32_t    size;                   /* File size in bytes */
    uint32_t    inode;                  /* Filesystem-specific inode number */

    /* Operation dispatch — NULL means unsupported */
    read_fn_t    read;
    write_fn_t   write;
    open_fn_t    open;
    close_fn_t   close;
    finddir_fn_t finddir;

    /* Directory children (inline for simplicity) */
    struct vfs_node *children[VFS_MAX_CHILDREN];
    uint32_t    child_count;

    /* Mountpoint: if set, lookups redirect to this node's subtree */
    struct vfs_node *mount;

    /* Filesystem-private data pointer */
    void        *private_data;
} vfs_node_t;

/* --- VFS API --- */

/* Initialize the VFS (creates root node) */
void vfs_init(void);

/* Get the VFS root node */
vfs_node_t *vfs_root(void);

/* Resolve a path to a VFS node. Returns NULL if not found. */
vfs_node_t *vfs_lookup(const char *path);

/* Read from a file node */
uint32_t vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size,
                  uint8_t *buffer);

/* Write to a file node */
uint32_t vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size,
                   const uint8_t *buffer);

/* Open a file node */
int vfs_open(vfs_node_t *node, uint32_t flags);

/* Close a file node */
int vfs_close(vfs_node_t *node);

/* Find an entry in a directory by name */
vfs_node_t *vfs_finddir(vfs_node_t *dir, const char *name);

/* Add a child node to a directory */
int vfs_add_child(vfs_node_t *parent, vfs_node_t *child);

/* Create a new VFS node (allocated from kernel heap) */
vfs_node_t *vfs_create_node(const char *name, uint32_t type);

#endif /* _ASTRA_KERNEL_VFS_H */
