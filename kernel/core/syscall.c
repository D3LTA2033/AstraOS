/* ==========================================================================
 * AstraOS - System Call Implementation
 * ==========================================================================
 * Handles int 0x80 from user space. The ISR stub pushes all registers,
 * and we extract syscall number from EAX, args from EBX-EDI.
 *
 * Phase 6 additions:
 *   - SYS_OPEN  (5): Open a file by path, returns fd
 *   - SYS_READ  (6): Read from an open fd
 *   - SYS_CLOSE (7): Close an open fd
 *   - SYS_WRITE updated to support VFS-backed fds (fd=1 still writes VGA)
 * ========================================================================== */

#include <kernel/syscall.h>
#include <kernel/idt.h>
#include <kernel/gdt.h>
#include <kernel/task.h>
#include <kernel/scheduler.h>
#include <kernel/vfs.h>
#include <kernel/usermode.h>
#include <kernel/capability.h>
#include <kernel/pipe.h>
#include <kernel/signal.h>
#include <kernel/kheap.h>
#include <kernel/kstring.h>
#include <drivers/vga.h>
#include <drivers/pit.h>

/* Boundary between user and kernel virtual address space */
#define USER_KERNEL_BOUNDARY 0xC0000000

/* Validate that a user buffer is entirely below the kernel boundary */
static int validate_user_buffer(uint32_t addr, uint32_t len)
{
    if (addr >= USER_KERNEL_BOUNDARY)
        return -1;
    if (len > 0 && addr + len - 1 >= USER_KERNEL_BOUNDARY)
        return -1;
    return 0;
}

/* Validate that a user string is below the kernel boundary.
 * Returns string length, or -1 on failure. */
static int validate_user_string(uint32_t addr)
{
    if (addr >= USER_KERNEL_BOUNDARY)
        return -1;
    const char *s = (const char *)addr;
    int len = 0;
    while (s[len] != '\0') {
        if (addr + (uint32_t)len + 1 >= USER_KERNEL_BOUNDARY)
            return -1;
        len++;
        if (len > (int)VFS_PATH_MAX)
            return -1;
    }
    return len;
}

/* --- fd helpers --- */

static int fd_alloc(task_t *task, vfs_node_t *node, uint32_t flags)
{
    for (int i = 0; i < TASK_MAX_FDS; i++) {
        if (task->fds[i].node == NULL) {
            task->fds[i].node = node;
            task->fds[i].offset = 0;
            task->fds[i].flags = flags;
            return i;
        }
    }
    return -1; /* No free fd */
}

static fd_entry_t *fd_get(task_t *task, int fd)
{
    if (fd < 0 || fd >= TASK_MAX_FDS)
        return NULL;
    if (task->fds[fd].node == NULL)
        return NULL;
    return &task->fds[fd];
}

/* --- Syscall implementations --- */

static uint32_t sys_exit(uint32_t code, uint32_t b, uint32_t c,
                         uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    task_t *cur = task_current();
    if (cur)
        cur->exit_code = code;
    task_exit();
    return 0;
}

static uint32_t sys_write(uint32_t fd, uint32_t buf_addr, uint32_t len,
                          uint32_t d, uint32_t e)
{
    (void)d; (void)e;

    if (validate_user_buffer(buf_addr, len) < 0)
        return (uint32_t)-1;

    const uint8_t *buf = (const uint8_t *)buf_addr;

    /* fd=1 (stdout) — legacy VGA console path */
    if (fd == 1) {
        task_t *cur = task_current();
        fd_entry_t *fde = cur ? fd_get(cur, 1) : NULL;

        /* If fd 1 is bound to a VFS node, use it */
        if (fde && fde->node) {
            uint32_t written = vfs_write(fde->node, fde->offset, len, buf);
            fde->offset += written;
            return written;
        }

        /* Fallback: direct VGA output */
        for (uint32_t i = 0; i < len; i++)
            vga_putchar((char)buf[i]);
        return len;
    }

    /* Other fds — go through VFS */
    task_t *cur2 = task_current();
    if (!cur2)
        return (uint32_t)-1;

    /* Check write capability */
    if (!cap_check(cur2->capabilities, CAP_VFS_WRITE)) {
        security_audit("SYSCALL", "write DENIED (no CAP_VFS_WRITE)",
                       cur2->id, fd);
        return (uint32_t)-1;
    }

    fd_entry_t *fde2 = fd_get(cur2, (int)fd);
    if (!fde2)
        return (uint32_t)-1;

    uint32_t written = vfs_write(fde2->node, fde2->offset, len, buf);
    fde2->offset += written;
    return written;
}

static uint32_t sys_getpid(uint32_t a, uint32_t b, uint32_t c,
                           uint32_t d, uint32_t e)
{
    (void)a; (void)b; (void)c; (void)d; (void)e;
    task_t *cur = task_current();
    return cur ? cur->id : 0;
}

static uint32_t sys_yield(uint32_t a, uint32_t b, uint32_t c,
                          uint32_t d, uint32_t e)
{
    (void)a; (void)b; (void)c; (void)d; (void)e;
    sched_yield();
    return 0;
}

static uint32_t sys_sleep(uint32_t ticks, uint32_t b, uint32_t c,
                          uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    uint32_t start = pit_get_ticks();
    while (pit_get_ticks() - start < ticks)
        __asm__ volatile ("sti; hlt");
    return 0;
}

static uint32_t sys_open(uint32_t path_addr, uint32_t flags, uint32_t c,
                         uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;

    task_t *cur = task_current();
    if (!cur)
        return (uint32_t)-1;

    int slen = validate_user_string(path_addr);
    if (slen < 0)
        return (uint32_t)-1;

    const char *path = (const char *)path_addr;
    vfs_node_t *node = vfs_lookup(path);
    if (!node)
        return (uint32_t)-1;

    /* Capability check: device nodes require CAP_DEVICE_ACCESS,
     * regular files require CAP_VFS_READ */
    if (node->type & VFS_CHARDEVICE) {
        if (!cap_check(cur->capabilities, CAP_DEVICE_ACCESS)) {
            security_audit("SYSCALL", "open DENIED (no CAP_DEVICE_ACCESS)",
                           cur->id, (uint32_t)path_addr);
            return (uint32_t)-1;
        }
    } else {
        if (!cap_check(cur->capabilities, CAP_VFS_READ)) {
            security_audit("SYSCALL", "open DENIED (no CAP_VFS_READ)",
                           cur->id, (uint32_t)path_addr);
            return (uint32_t)-1;
        }
    }

    if (vfs_open(node, flags) < 0)
        return (uint32_t)-1;

    int fd = fd_alloc(cur, node, flags);
    if (fd < 0) {
        vfs_close(node);
        return (uint32_t)-1;
    }

    return (uint32_t)fd;
}

static uint32_t sys_read(uint32_t fd, uint32_t buf_addr, uint32_t len,
                         uint32_t d, uint32_t e)
{
    (void)d; (void)e;

    if (validate_user_buffer(buf_addr, len) < 0)
        return (uint32_t)-1;

    task_t *cur = task_current();
    if (!cur)
        return (uint32_t)-1;

    /* Check read capability */
    if (!cap_check(cur->capabilities, CAP_VFS_READ)) {
        security_audit("SYSCALL", "read DENIED (no CAP_VFS_READ)",
                       cur->id, fd);
        return (uint32_t)-1;
    }

    fd_entry_t *fde = fd_get(cur, (int)fd);
    if (!fde)
        return (uint32_t)-1;

    uint8_t *buf = (uint8_t *)buf_addr;
    uint32_t nread = vfs_read(fde->node, fde->offset, len, buf);
    fde->offset += nread;
    return nread;
}

static uint32_t sys_close(uint32_t fd, uint32_t b, uint32_t c,
                          uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;

    task_t *cur = task_current();
    if (!cur)
        return (uint32_t)-1;

    fd_entry_t *fde = fd_get(cur, (int)fd);
    if (!fde)
        return (uint32_t)-1;

    vfs_close(fde->node);
    fde->node = NULL;
    fde->offset = 0;
    fde->flags = 0;
    return 0;
}

/* SYS_STAT: get file type and size.
 * Returns size in eax. Type is written to *ecx if ecx != 0. */
static uint32_t sys_stat(uint32_t path_addr, uint32_t type_out, uint32_t c,
                         uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;

    int slen = validate_user_string(path_addr);
    if (slen < 0)
        return (uint32_t)-1;

    const char *path = (const char *)path_addr;
    vfs_node_t *node = vfs_lookup(path);
    if (!node)
        return (uint32_t)-1;

    if (type_out && type_out < USER_KERNEL_BOUNDARY) {
        *(uint32_t *)type_out = node->type;
    }

    return node->size;
}

/* SYS_READDIR: read directory entry at index.
 * ebx=path_addr, ecx=index, edx=name_buf, esi=buf_size
 * Returns name length, or 0 if no more entries. */
static uint32_t sys_readdir(uint32_t path_addr, uint32_t index,
                            uint32_t name_buf, uint32_t buf_size,
                            uint32_t e)
{
    (void)e;

    int slen = validate_user_string(path_addr);
    if (slen < 0)
        return 0;

    if (validate_user_buffer(name_buf, buf_size) < 0)
        return 0;

    const char *path = (const char *)path_addr;
    vfs_node_t *node = vfs_lookup(path);
    if (!node || !(node->type & VFS_DIRECTORY))
        return 0;

    if (index >= node->child_count)
        return 0;

    vfs_node_t *child = node->children[index];
    if (!child)
        return 0;

    uint32_t nlen = kstrlen(child->name);
    if (nlen >= buf_size)
        nlen = buf_size - 1;

    kmemcpy((void *)name_buf, child->name, nlen);
    ((char *)name_buf)[nlen] = '\0';

    return nlen;
}

/* SYS_SPAWN: spawn a new user task from a ramfs binary.
 * ebx=path_addr. Returns child task ID, or -1 on failure. */
static uint32_t sys_spawn(uint32_t path_addr, uint32_t b, uint32_t c,
                          uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;

    task_t *cur = task_current();
    if (!cur)
        return (uint32_t)-1;

    if (!cap_check(cur->capabilities, CAP_PROC_CREATE)) {
        security_audit("SYSCALL", "spawn DENIED (no CAP_PROC_CREATE)",
                       cur->id, 0);
        return (uint32_t)-1;
    }

    int slen = validate_user_string(path_addr);
    if (slen < 0)
        return (uint32_t)-1;

    const char *path = (const char *)path_addr;
    vfs_node_t *node = vfs_lookup(path);
    if (!node || !(node->type & VFS_FILE))
        return (uint32_t)-1;

    /* Read the binary from ramfs into a kernel buffer */
    if (node->size == 0)
        return (uint32_t)-1;

    uint8_t *bin = (uint8_t *)kmalloc(node->size);
    if (!bin)
        return (uint32_t)-1;

    uint32_t nread = vfs_read(node, 0, node->size, bin);
    if (nread == 0) {
        kfree(bin);
        return (uint32_t)-1;
    }

    uint32_t tid = usermode_task_create(bin, nread);
    kfree(bin);

    if (!tid)
        return (uint32_t)-1;

    /* Set parent ID on the child */
    task_t *child = task_find(tid);
    if (child)
        child->parent_id = cur->id;

    security_audit("SYSCALL", "spawn OK", cur->id, tid);
    return tid;
}

/* SYS_WAITPID: wait for a child task to exit.
 * ebx=child_tid. Returns child's exit code, or -1 on failure. */
static uint32_t sys_waitpid(uint32_t child_tid, uint32_t b, uint32_t c,
                            uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;

    task_t *cur = task_current();
    if (!cur)
        return (uint32_t)-1;

    task_t *child = task_find(child_tid);
    if (!child)
        return (uint32_t)-1;

    /* Only the parent can wait on a child */
    if (child->parent_id != cur->id)
        return (uint32_t)-1;

    /* If already dead, return immediately */
    if (child->state == TASK_DEAD)
        return child->exit_code;

    /* Block until child exits */
    cur->wait_for = child_tid;
    cur->state = TASK_BLOCKED;
    schedule();

    /* Woken up — child should be dead now */
    child = task_find(child_tid);
    if (child)
        return child->exit_code;

    return 0;
}

/* SYS_PIPE: create a pipe.
 * ebx=fd_out (pointer to uint32_t[2]: [0]=read fd, [1]=write fd).
 * Returns 0 on success, -1 on failure. */
static uint32_t sys_pipe(uint32_t fd_out_addr, uint32_t b, uint32_t c,
                         uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;

    if (validate_user_buffer(fd_out_addr, 8) < 0)
        return (uint32_t)-1;

    task_t *cur = task_current();
    if (!cur)
        return (uint32_t)-1;

    vfs_node_t *rn = NULL, *wn = NULL;
    if (pipe_create(&rn, &wn) < 0)
        return (uint32_t)-1;

    int rfd = fd_alloc(cur, rn, 0);
    int wfd = fd_alloc(cur, wn, 0);
    if (rfd < 0 || wfd < 0) {
        /* Clean up on failure */
        if (rfd >= 0) { cur->fds[rfd].node = NULL; }
        if (wfd >= 0) { cur->fds[wfd].node = NULL; }
        kfree(rn);
        kfree(wn);
        return (uint32_t)-1;
    }

    uint32_t *fd_out = (uint32_t *)fd_out_addr;
    fd_out[0] = (uint32_t)rfd;
    fd_out[1] = (uint32_t)wfd;
    return 0;
}

/* SYS_KILL: send a signal to a task.
 * ebx=target_tid, ecx=signal number. Returns 0 on success. */
static uint32_t sys_kill(uint32_t target_tid, uint32_t sig, uint32_t c,
                         uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;

    task_t *cur = task_current();
    if (!cur)
        return (uint32_t)-1;

    if (!cap_check(cur->capabilities, CAP_SIGNAL)) {
        security_audit("SYSCALL", "kill DENIED (no CAP_SIGNAL)",
                       cur->id, target_tid);
        return (uint32_t)-1;
    }

    return (uint32_t)signal_send(cur->id, target_tid, (int)sig);
}

/* SYS_GETPPID: get parent task ID. */
static uint32_t sys_getppid(uint32_t a, uint32_t b, uint32_t c,
                            uint32_t d, uint32_t e)
{
    (void)a; (void)b; (void)c; (void)d; (void)e;
    task_t *cur = task_current();
    return cur ? cur->parent_id : 0;
}

/* SYS_REBOOT: reboot the system via keyboard controller reset. */
static uint32_t sys_reboot(uint32_t a, uint32_t b, uint32_t c,
                           uint32_t d, uint32_t e)
{
    (void)a; (void)b; (void)c; (void)d; (void)e;

    task_t *cur = task_current();
    if (cur) {
        security_audit("SYSCALL", "reboot requested", cur->id, 0);
    }

    /* Triple fault method: load a zero-length IDT and trigger an interrupt */
    __asm__ volatile ("cli");

    /* Attempt keyboard controller reset (0xFE to port 0x64) */
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0xFE), "Nd"((uint16_t)0x64));

    /* If that didn't work, triple fault */
    struct { uint16_t limit; uint32_t base; } __attribute__((packed)) null_idt = {0, 0};
    __asm__ volatile ("lidt %0; int $3" : : "m"(null_idt));

    /* Should never reach here */
    while (1) __asm__ volatile ("hlt");
    return 0;
}

/* --- Dispatch table --- */

static syscall_fn_t syscall_table[SYSCALL_COUNT] = {
    [SYS_EXIT]    = sys_exit,
    [SYS_WRITE]   = sys_write,
    [SYS_GETPID]  = sys_getpid,
    [SYS_YIELD]   = sys_yield,
    [SYS_SLEEP]   = sys_sleep,
    [SYS_OPEN]    = sys_open,
    [SYS_READ]    = sys_read,
    [SYS_CLOSE]   = sys_close,
    [SYS_STAT]    = sys_stat,
    [SYS_READDIR] = sys_readdir,
    [SYS_SPAWN]   = sys_spawn,
    [SYS_WAITPID] = sys_waitpid,
    [SYS_PIPE]    = sys_pipe,
    [SYS_KILL]    = sys_kill,
    [SYS_GETPPID] = sys_getppid,
    [SYS_REBOOT]  = sys_reboot,
};

/* --- ISR handler for int 0x80 --- */

static void syscall_handler(registers_t *regs)
{
    uint32_t num = regs->eax;

    if (num >= SYSCALL_COUNT) {
        regs->eax = (uint32_t)-1;
        return;
    }

    syscall_fn_t fn = syscall_table[num];
    if (!fn) {
        regs->eax = (uint32_t)-1;
        return;
    }

    regs->eax = fn(regs->ebx, regs->ecx, regs->edx, regs->esi, regs->edi);
}

void syscall_init(void)
{
    idt_set_gate(SYSCALL_VECTOR, (uint32_t)isr128, GDT_KERNEL_CODE, IDT_GATE_USER);
    isr_register_handler(SYSCALL_VECTOR, syscall_handler);
}
