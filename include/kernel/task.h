/* ==========================================================================
 * AstraOS - Task (Process/Thread) Management
 * ==========================================================================
 * Each task has:
 *   - A unique ID
 *   - A saved CPU context (registers + stack pointer)
 *   - Its own kernel stack (with guard page for overflow detection)
 *   - A state (running, ready, blocked, dead)
 *   - A capability bitmask controlling access rights
 *   - An optional per-task page directory for address space isolation
 *   - A file descriptor table for VFS access
 *
 * Context switching works by:
 *   1. Saving current task's ESP — all registers are on stack
 *   2. Switching CR3 if the new task has a different address space
 *   3. Loading new task's ESP
 *   4. Returning from the switch pops the new task's registers
 * ========================================================================== */

#ifndef _ASTRA_KERNEL_TASK_H
#define _ASTRA_KERNEL_TASK_H

#include <kernel/types.h>

/* Forward-declare to avoid circular includes */
struct vfs_node;
struct page_directory;

/* Kernel stack size per task: 8KB (2 pages) */
#define TASK_STACK_SIZE     8192

/* Guard page size at the bottom of each kernel stack */
#define TASK_GUARD_SIZE     4096

/* Maximum number of tasks */
#define MAX_TASKS           64

/* Maximum open file descriptors per task */
#define TASK_MAX_FDS        16

/* Task states */
typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_DEAD,
} task_state_t;

/* Open file descriptor entry */
typedef struct {
    struct vfs_node *node;  /* VFS node this fd points to, NULL if unused */
    uint32_t         offset; /* Current read/write position */
    uint32_t         flags;  /* Open flags */
} fd_entry_t;

/* Saved CPU context — matches what context_switch pushes/pops */
typedef struct {
    uint32_t esp;       /* Stack pointer at point of switch */
    uint32_t ebp;
    uint32_t ebx;
    uint32_t esi;
    uint32_t edi;
    uint32_t eflags;
} task_context_t;

/* Task Control Block (TCB) */
typedef struct task {
    uint32_t        id;             /* Unique task ID */
    task_state_t    state;          /* Current state */
    task_context_t  context;        /* Saved CPU context */
    uint32_t        stack_base;     /* Bottom of kernel stack (for freeing) */
    uint32_t        stack_top;      /* Top of kernel stack */
    uint32_t        capabilities;   /* Capability bitmask (Phase 7) */
    void           *page_dir;       /* Per-task page directory, NULL = kernel */
    uint32_t        page_dir_phys;  /* Physical address of page directory for CR3 */
    fd_entry_t      fds[TASK_MAX_FDS]; /* Per-task file descriptor table */
    uint32_t        parent_id;      /* Parent task ID (0 = none) */
    uint32_t        exit_code;      /* Exit code set by SYS_EXIT */
    uint32_t        wait_for;       /* Task ID we're waiting on (0 = none) */
    struct task     *next;          /* Next task in scheduler queue */
} task_t;

/* Task entry point function signature */
typedef void (*task_entry_t)(void);

/* Create a new kernel task. Returns task ID, or 0 on failure. */
uint32_t task_create(task_entry_t entry);

/* Terminate the current task */
void task_exit(void);

/* Get the currently running task */
task_t *task_current(void);

/* Find a task by ID. Returns NULL if not found. */
task_t *task_find(uint32_t tid);

/* Assembly context switch: switch from *old_esp to new_esp */
extern void context_switch(uint32_t *old_esp, uint32_t new_esp);

#endif /* _ASTRA_KERNEL_TASK_H */
