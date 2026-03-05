/* ==========================================================================
 * AstraOS - Kernel Pipe Implementation
 * ==========================================================================
 * Ring-buffer pipe with VFS node wrappers. Read blocks (yields) when empty
 * until data is available or the write end is closed. Write blocks when full.
 * ========================================================================== */

#include <kernel/pipe.h>
#include <kernel/kheap.h>
#include <kernel/kstring.h>
#include <kernel/scheduler.h>

static uint32_t pipe_read(vfs_node_t *node, uint32_t offset,
                          uint32_t size, uint8_t *buffer)
{
    (void)offset;
    pipe_t *p = (pipe_t *)node->private_data;
    if (!p) return 0;

    uint32_t total = 0;
    while (total < size) {
        if (p->count == 0) {
            if (p->write_closed)
                break;  /* EOF — writer is gone */
            /* Yield and retry */
            sched_yield();
            continue;
        }
        buffer[total++] = p->buf[p->read_pos];
        p->read_pos = (p->read_pos + 1) % PIPE_BUF_SIZE;
        p->count--;
    }
    return total;
}

static uint32_t pipe_write(vfs_node_t *node, uint32_t offset,
                           uint32_t size, const uint8_t *buffer)
{
    (void)offset;
    pipe_t *p = (pipe_t *)node->private_data;
    if (!p) return 0;

    uint32_t total = 0;
    while (total < size) {
        if (p->read_closed)
            break;  /* Reader is gone — broken pipe */
        if (p->count >= PIPE_BUF_SIZE) {
            /* Buffer full — yield and retry */
            sched_yield();
            continue;
        }
        p->buf[p->write_pos] = buffer[total++];
        p->write_pos = (p->write_pos + 1) % PIPE_BUF_SIZE;
        p->count++;
    }
    return total;
}

static int pipe_close_read(vfs_node_t *node)
{
    pipe_t *p = (pipe_t *)node->private_data;
    if (p) p->read_closed = true;
    return 0;
}

static int pipe_close_write(vfs_node_t *node)
{
    pipe_t *p = (pipe_t *)node->private_data;
    if (p) p->write_closed = true;
    return 0;
}

int pipe_create(vfs_node_t **read_node, vfs_node_t **write_node)
{
    pipe_t *p = (pipe_t *)kmalloc(sizeof(pipe_t));
    if (!p) return -1;
    kmemset(p, 0, sizeof(pipe_t));

    vfs_node_t *rn = vfs_create_node("pipe_r", VFS_PIPE);
    vfs_node_t *wn = vfs_create_node("pipe_w", VFS_PIPE);
    if (!rn || !wn) {
        if (rn) kfree(rn);
        if (wn) kfree(wn);
        kfree(p);
        return -1;
    }

    rn->private_data = p;
    rn->read  = pipe_read;
    rn->close = pipe_close_read;

    wn->private_data = p;
    wn->write = pipe_write;
    wn->close = pipe_close_write;

    *read_node  = rn;
    *write_node = wn;
    return 0;
}
