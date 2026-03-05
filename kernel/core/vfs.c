/* ==========================================================================
 * AstraOS - Virtual File System Implementation
 * ==========================================================================
 * Implements path resolution, node creation, and dispatch to filesystem-
 * specific operation handlers.
 * ========================================================================== */

#include <kernel/vfs.h>
#include <kernel/kheap.h>
#include <kernel/kstring.h>

/* The root of the entire VFS tree */
static vfs_node_t *vfs_root_node = NULL;

void vfs_init(void)
{
    vfs_root_node = vfs_create_node("/", VFS_DIRECTORY);
}

vfs_node_t *vfs_root(void)
{
    return vfs_root_node;
}

vfs_node_t *vfs_create_node(const char *name, uint32_t type)
{
    vfs_node_t *node = (vfs_node_t *)kcalloc(1, sizeof(vfs_node_t));
    if (!node)
        return NULL;

    size_t len = kstrlen(name);
    if (len > VFS_NAME_MAX)
        len = VFS_NAME_MAX;
    kmemcpy(node->name, name, len);
    node->name[len] = '\0';
    node->type = type;

    return node;
}

int vfs_add_child(vfs_node_t *parent, vfs_node_t *child)
{
    if (!parent || !child)
        return -1;
    if (!(parent->type & VFS_DIRECTORY))
        return -1;
    if (parent->child_count >= VFS_MAX_CHILDREN)
        return -1;

    parent->children[parent->child_count++] = child;
    return 0;
}

vfs_node_t *vfs_finddir(vfs_node_t *dir, const char *name)
{
    if (!dir || !(dir->type & VFS_DIRECTORY))
        return NULL;

    /* If this directory has a custom finddir, use it */
    if (dir->finddir)
        return dir->finddir(dir, name);

    /* Default: linear scan of children */
    for (uint32_t i = 0; i < dir->child_count; i++) {
        if (kstrcmp(dir->children[i]->name, name) == 0)
            return dir->children[i];
    }
    return NULL;
}

/* Split the next path component from a path string.
 * On entry, *path points to the start (past any leading '/').
 * On return, component holds the next name, *path advances past it. */
static int path_next(const char **path, char *component, size_t comp_size)
{
    const char *p = *path;

    /* Skip leading slashes */
    while (*p == '/')
        p++;

    if (*p == '\0')
        return 0; /* No more components */

    size_t i = 0;
    while (*p != '\0' && *p != '/' && i < comp_size - 1)
        component[i++] = *p++;
    component[i] = '\0';

    *path = p;
    return 1;
}

vfs_node_t *vfs_lookup(const char *path)
{
    if (!path || !vfs_root_node)
        return NULL;

    /* Root itself */
    if (path[0] == '/' && path[1] == '\0')
        return vfs_root_node;

    vfs_node_t *current = vfs_root_node;
    char component[VFS_NAME_MAX + 1];

    while (path_next(&path, component, sizeof(component))) {
        /* Follow mount points */
        if (current->mount)
            current = current->mount;

        current = vfs_finddir(current, component);
        if (!current)
            return NULL;

        /* Follow mount points on the found node too */
        if (current->mount)
            current = current->mount;
    }

    return current;
}

uint32_t vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size,
                  uint8_t *buffer)
{
    if (!node || !node->read)
        return 0;
    return node->read(node, offset, size, buffer);
}

uint32_t vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size,
                   const uint8_t *buffer)
{
    if (!node || !node->write)
        return 0;
    return node->write(node, offset, size, buffer);
}

int vfs_open(vfs_node_t *node, uint32_t flags)
{
    if (!node)
        return -1;
    if (node->open)
        return node->open(node, flags);
    return 0; /* Default: success */
}

int vfs_close(vfs_node_t *node)
{
    if (!node)
        return -1;
    if (node->close)
        return node->close(node);
    return 0;
}
