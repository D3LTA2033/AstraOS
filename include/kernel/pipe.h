/* ==========================================================================
 * AstraOS - Kernel Pipe
 * ==========================================================================
 * Unidirectional byte stream between tasks. Uses a fixed-size ring buffer
 * in kernel heap memory. Writers block when full, readers block when empty.
 * ========================================================================== */

#ifndef _ASTRA_KERNEL_PIPE_H
#define _ASTRA_KERNEL_PIPE_H

#include <kernel/types.h>
#include <kernel/vfs.h>

#define PIPE_BUF_SIZE 512

typedef struct {
    uint8_t  buf[PIPE_BUF_SIZE];
    uint32_t read_pos;
    uint32_t write_pos;
    uint32_t count;         /* Bytes currently in buffer */
    bool     write_closed;  /* Writer has closed their end */
    bool     read_closed;   /* Reader has closed their end */
} pipe_t;

/* Create a pipe, returning read and write VFS nodes.
 * Returns 0 on success, -1 on failure. */
int pipe_create(vfs_node_t **read_node, vfs_node_t **write_node);

#endif /* _ASTRA_KERNEL_PIPE_H */
