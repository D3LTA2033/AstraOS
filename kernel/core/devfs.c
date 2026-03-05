/* ==========================================================================
 * AstraOS - Device Filesystem Implementation
 * ==========================================================================
 * Creates /dev as a VFS directory and provides an API for drivers to
 * register device nodes with custom operation handlers.
 * ========================================================================== */

#include <kernel/devfs.h>
#include <kernel/vfs.h>
#include <kernel/kheap.h>
#include <kernel/kstring.h>

static vfs_node_t *devfs_root = NULL;

void devfs_init(void)
{
    /* Create /dev directory and attach to VFS root */
    devfs_root = vfs_create_node("dev", VFS_DIRECTORY);
    if (!devfs_root)
        return;

    vfs_node_t *root = vfs_root();
    if (!root)
        return;

    vfs_add_child(root, devfs_root);
}

int devfs_register_chardev(const char *name,
                           read_fn_t  read_op,
                           write_fn_t write_op,
                           open_fn_t  open_op,
                           close_fn_t close_op)
{
    if (!devfs_root || !name)
        return -1;

    vfs_node_t *dev = vfs_create_node(name, VFS_CHARDEVICE);
    if (!dev)
        return -1;

    dev->read  = read_op;
    dev->write = write_op;
    dev->open  = open_op;
    dev->close = close_op;

    return vfs_add_child(devfs_root, dev);
}
