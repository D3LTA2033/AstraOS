/* ==========================================================================
 * AstraOS - Task (Process/Thread) Management
 * ==========================================================================
 * Each task has:
 *   - A unique ID
 *   - A saved CPU context (registers + stack pointer)
 *   - Its own kernel stack
 *   - A state (running, ready, blocked, dead)
 *
 * For Phase 4, all tasks run in ring 0 (kernel threads).
 * User-mode tasks come in Phase 5.
 *
 * Context switching works by:
 *   1. Saving current task's ESP (stack pointer) — all registers are on stack
 *   2. Loading new task's ESP
 *   3. Returning from the switch pops the new task's registers
 * ========================================================================== */

#ifndef _ASTRA_KERNEL_TASK_H
#define _ASTRA_KERNEL_TASK_H

#include <kernel/types.h>

/* Kernel stack size per task: 8KB */
#define TASK_STACK_SIZE     8192

/* Maximum number of tasks (static limit for Phase 4, dynamic in Phase 6) */
#define MAX_TASKS           64

/* Task states */
typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_DEAD,
} task_state_t;

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

/* Assembly context switch: switch from *old_esp to new_esp */
extern void context_switch(uint32_t *old_esp, uint32_t new_esp);

#endif /* _ASTRA_KERNEL_TASK_H */
