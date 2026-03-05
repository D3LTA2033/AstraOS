/* ==========================================================================
 * AstraOS - User Mode Implementation
 * ==========================================================================
 * Creates user-space tasks by:
 *   1. Allocating physical frames for user code and stack
 *   2. Mapping them into the kernel page directory with PAGE_USER flags
 *   3. Copying the user program into the mapped pages
 *   4. Creating a kernel task whose entry function does enter_usermode()
 *
 * User programs are loaded at a fixed virtual address (0x00400000).
 * User stack grows down from 0x00800000.
 *
 * For Phase 5, user tasks share the kernel page directory (no per-process
 * address spaces yet — that comes with fork/exec in Phase 6).
 * ========================================================================== */

#include <kernel/usermode.h>
#include <kernel/vmm.h>
#include <kernel/pmm.h>
#include <kernel/task.h>
#include <kernel/gdt.h>
#include <kernel/kheap.h>
#include <kernel/kstring.h>
#include <drivers/vga.h>

/* User-space memory layout */
#define USER_CODE_BASE  0x00400000
#define USER_STACK_TOP  0x00800000
#define USER_STACK_SIZE (4 * PAGE_SIZE)     /* 16KB user stack */

/* Per-user-task info stored in kernel memory */
typedef struct {
    uint32_t entry;
    uint32_t user_esp;
} user_task_info_t;

/* Kernel-side entry point for a user task.
 * This function runs in ring 0, sets up the TSS stack, then IRETs to ring 3. */
static void user_task_kernel_entry(void)
{
    task_t *cur = task_current();

    /* Update TSS with this task's kernel stack (for ring 3 -> 0 transitions) */
    tss_set_kernel_stack(cur->stack_top);

    /* The user entry and stack are encoded in the task's stack area.
     * We stored them at a known offset during creation. */
    user_task_info_t *info = (user_task_info_t *)(cur->stack_base);

    enter_usermode(info->entry, info->user_esp);

    /* Should never return */
    task_exit();
}

void usermode_init(void)
{
    /* Nothing to do for Phase 5 — setup happens per-task */
}

uint32_t usermode_task_create(const void *code_src, size_t code_size)
{
    page_directory_t *dir = vmm_get_kernel_directory();

    /* Map user code pages at USER_CODE_BASE with user-accessible flags */
    uint32_t code_pages = (code_size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t i = 0; i < code_pages; i++) {
        uint32_t frame = pmm_alloc_frame();
        if (!frame) return 0;
        vmm_map_page(dir, USER_CODE_BASE + i * PAGE_SIZE, frame, PAGE_USERMODE);
    }

    /* Copy user program to the mapped address */
    kmemcpy((void *)USER_CODE_BASE, code_src, code_size);

    /* Map user stack pages (grows down from USER_STACK_TOP) */
    uint32_t stack_pages = USER_STACK_SIZE / PAGE_SIZE;
    uint32_t stack_base = USER_STACK_TOP - USER_STACK_SIZE;
    for (uint32_t i = 0; i < stack_pages; i++) {
        uint32_t frame = pmm_alloc_frame();
        if (!frame) return 0;
        vmm_map_page(dir, stack_base + i * PAGE_SIZE, frame, PAGE_USERMODE);
    }

    /* Create a kernel task that will transition to user mode */
    uint32_t tid = task_create(user_task_kernel_entry);
    if (!tid) return 0;

    /* Find the task and store user info at its stack_base.
     * task_create allocated stack_base via kmalloc, so we can use the
     * beginning of that buffer for our info struct. The actual kernel
     * stack starts from the top and grows down, so there's no conflict. */
    task_t *t = task_current();
    /* Walk to find the task we just created */
    task_t *walk = t;
    do {
        if (walk->id == tid) {
            user_task_info_t *info = (user_task_info_t *)(walk->stack_base);
            info->entry    = USER_CODE_BASE;
            info->user_esp = USER_STACK_TOP - 16; /* Leave room for initial frame */
            break;
        }
        walk = walk->next;
    } while (walk != t);

    return tid;
}
