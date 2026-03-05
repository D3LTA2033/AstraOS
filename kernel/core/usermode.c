/* ==========================================================================
 * AstraOS - User Mode Implementation
 * ==========================================================================
 * Phase 7: Each user task now gets its own page directory for full address
 * space isolation. Kernel mappings are shared (cloned from the kernel
 * directory), but user-space mappings are private per task.
 *
 * The creation sequence:
 *   1. Create a new page directory (clones kernel mappings)
 *   2. Map user code pages into the new directory
 *   3. Copy the user program binary into those pages
 *   4. Map user stack pages into the new directory
 *   5. Create a kernel task with restricted capabilities
 *   6. Assign the page directory to the task
 *   7. When scheduled, CR3 is loaded with this task's directory
 *   8. The kernel entry function transitions to ring 3 via IRET
 * ========================================================================== */

#include <kernel/usermode.h>
#include <kernel/vmm.h>
#include <kernel/pmm.h>
#include <kernel/task.h>
#include <kernel/gdt.h>
#include <kernel/capability.h>
#include <kernel/kheap.h>
#include <kernel/kstring.h>
#include <drivers/vga.h>
#include <drivers/serial.h>

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

    user_task_info_t *info = (user_task_info_t *)(cur->stack_base);

    /* Log the ring transition */
    serial_write("[AUDIT] USERMODE: ring0->ring3 transition (task=");
    char tbuf[12];
    int ti = 0;
    uint32_t tv = cur->id;
    if (tv == 0) { tbuf[ti++] = '0'; }
    else { while (tv > 0) { tbuf[ti++] = '0' + (char)(tv % 10); tv /= 10; } }
    for (int j = ti - 1; j >= 0; j--) serial_putchar(tbuf[j]);
    serial_write(", caps=0x");
    const char hex[] = "0123456789ABCDEF";
    for (int i = 28; i >= 0; i -= 4)
        serial_putchar(hex[(cur->capabilities >> i) & 0xF]);
    serial_write(")\r\n");

    enter_usermode(info->entry, info->user_esp);

    /* Should never return */
    task_exit();
}

void usermode_init(void)
{
    /* Nothing to do — setup happens per-task */
}

uint32_t usermode_task_create(const void *code_src, size_t code_size)
{
    /* Create a private address space for this user task */
    uint32_t dir_phys = 0;
    page_directory_t *dir = vmm_create_user_directory(&dir_phys);
    if (!dir) {
        vga_set_color(vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
        vga_write("[FAIL] Cannot create user address space\n");
        return 0;
    }

    /* Map user code + BSS pages in the new directory.
     * Flat binaries don't include BSS, so we add extra pages for static data. */
    uint32_t code_pages = (code_size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t bss_pages  = 4;  /* 16KB for BSS (static buffers, etc.) */
    uint32_t total_code_pages = code_pages + bss_pages;
    for (uint32_t i = 0; i < total_code_pages; i++) {
        uint32_t frame = pmm_alloc_frame();
        if (!frame) {
            vmm_destroy_user_directory(dir);
            return 0;
        }
        vmm_map_page(dir, USER_CODE_BASE + i * PAGE_SIZE, frame, PAGE_USERMODE);
    }

    /* Copy user program into mapped pages and zero BSS.
     * We write via the frame's physical address (identity-mapped). */
    for (uint32_t i = 0; i < total_code_pages; i++) {
        uint32_t frame = vmm_get_physical(dir, USER_CODE_BASE + i * PAGE_SIZE);
        uint32_t phys = frame & 0xFFFFF000;
        uint32_t offset = i * PAGE_SIZE;

        /* Zero the entire frame first (covers BSS) */
        kmemset((void *)phys, 0, PAGE_SIZE);

        /* Copy code/data if within binary range */
        if (offset < code_size) {
            uint32_t chunk = code_size - offset;
            if (chunk > PAGE_SIZE)
                chunk = PAGE_SIZE;
            kmemcpy((void *)phys, (const uint8_t *)code_src + offset, chunk);
        }
    }

    /* Map user stack pages in the new directory */
    uint32_t stack_pages = USER_STACK_SIZE / PAGE_SIZE;
    uint32_t stack_base = USER_STACK_TOP - USER_STACK_SIZE;
    for (uint32_t i = 0; i < stack_pages; i++) {
        uint32_t frame = pmm_alloc_frame();
        if (!frame) {
            vmm_destroy_user_directory(dir);
            return 0;
        }
        vmm_map_page(dir, stack_base + i * PAGE_SIZE, frame, PAGE_USERMODE);

        /* Zero the stack frame (via identity-mapped physical address) */
        kmemset((void *)frame, 0, PAGE_SIZE);
    }

    /* Create a kernel task that will transition to user mode */
    uint32_t tid = task_create(user_task_kernel_entry);
    if (!tid) {
        vmm_destroy_user_directory(dir);
        return 0;
    }

    /* Find the task we just created and configure it */
    task_t *cur = task_current();
    task_t *walk = cur;
    do {
        if (walk->id == tid) {
            /* Store user info at the task's stack base */
            user_task_info_t *info = (user_task_info_t *)(walk->stack_base);
            info->entry    = USER_CODE_BASE;
            info->user_esp = USER_STACK_TOP - 16;

            /* Assign per-task page directory */
            walk->page_dir      = dir;
            walk->page_dir_phys = dir_phys;

            /* Restrict capabilities: user tasks get limited rights */
            walk->capabilities  = CAP_USER_DEFAULT;

            break;
        }
        walk = walk->next;
    } while (walk != cur);

    security_audit("USERMODE", "task created with isolated address space",
                   tid, dir_phys);

    return tid;
}
