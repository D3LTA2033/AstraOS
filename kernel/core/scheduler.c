/* ==========================================================================
 * AstraOS - Scheduler Implementation
 * ==========================================================================
 * Round-robin preemptive scheduler with circular ready queue.
 *
 * How context switching works:
 *   1. Timer fires -> schedule() is called
 *   2. We find the next READY task in the circular list
 *   3. context_switch() saves current ESP, loads new ESP
 *   4. The `ret` in context_switch jumps to wherever the new task left off
 *
 * New task bootstrapping:
 *   When task_create() sets up a new task, it prepares the stack to look
 *   like context_switch() just saved the registers, with the return address
 *   pointing to a trampoline that calls the actual entry function.
 * ========================================================================== */

#include <kernel/scheduler.h>
#include <kernel/task.h>
#include <kernel/idt.h>
#include <kernel/gdt.h>
#include <kernel/pic.h>
#include <kernel/kheap.h>
#include <kernel/kstring.h>
#include <drivers/vga.h>
#include <drivers/pit.h>

/* Circular linked list of all tasks */
static task_t *task_list_head = NULL;
static task_t *current_task   = NULL;
static uint32_t next_task_id  = 1;
static uint32_t sched_ticks   = 0;
static uint32_t active_tasks  = 0;
static bool     scheduler_active = false;

/* The idle task — runs when nothing else is ready */
static void idle_task_entry(void)
{
    while (1) {
        __asm__ volatile ("sti; hlt");
    }
}

/* Trampoline for new tasks — entry point is in EBX (set by task_create).
 * Defined as a global asm symbol so the linker can see it. */
__asm__(
    ".text\n"
    ".global task_trampoline\n"
    "task_trampoline:\n"
    "   sti\n"
    "   call *%ebx\n"
    "   call task_exit\n"
    "   jmp task_trampoline\n"
);
extern void task_trampoline(void);

/* Find the next runnable task after 'from' in the circular list */
static task_t *find_next_ready(task_t *from)
{
    task_t *t = from->next;
    task_t *start = t;

    do {
        if (t->state == TASK_READY)
            return t;
        t = t->next;
    } while (t != start);

    return from;
}

/* Clean up dead tasks from the list */
static void reap_dead_tasks(void)
{
    if (!task_list_head)
        return;

    task_t *prev = task_list_head;
    while (prev->next != task_list_head)
        prev = prev->next;

    task_t *cur = task_list_head;
    task_t *start = cur;
    bool first_pass = true;

    while (first_pass || cur != start) {
        first_pass = false;
        task_t *next = cur->next;

        if (cur->state == TASK_DEAD && cur != current_task) {
            if (cur->next == cur) {
                task_list_head = NULL;
                break;
            }
            if (cur == task_list_head) {
                task_list_head = next;
                start = next;
            }
            prev->next = next;

            if (cur->stack_base)
                kfree((void *)cur->stack_base);
            kfree(cur);
            active_tasks--;

            cur = next;
        } else {
            prev = cur;
            cur = next;
        }
    }
}

void schedule(void)
{
    if (!scheduler_active || !current_task)
        return;

    reap_dead_tasks();

    task_t *next = find_next_ready(current_task);
    if (next == current_task && current_task->state == TASK_RUNNING)
        return;

    task_t *prev = current_task;

    if (prev->state == TASK_RUNNING)
        prev->state = TASK_READY;

    next->state = TASK_RUNNING;
    current_task = next;

    tss_set_kernel_stack(next->stack_top);

    context_switch(&prev->context.esp, next->context.esp);
}

/* PIT timer hook — called every tick from ISR.
 * Must also update the PIT tick counter since we replaced the original handler. */
static void scheduler_tick(registers_t *regs)
{
    (void)regs;
    pit_tick();
    pic_send_eoi(0);

    if (!scheduler_active)
        return;

    sched_ticks++;
    if (sched_ticks >= SCHED_QUANTUM) {
        sched_ticks = 0;
        schedule();
    }
}

void sched_yield(void)
{
    schedule();
}

uint32_t sched_task_count(void)
{
    return active_tasks;
}

task_t *task_current(void)
{
    return current_task;
}

/* Add a task to the circular list */
static void task_list_add(task_t *task)
{
    if (!task_list_head) {
        task_list_head = task;
        task->next = task;
    } else {
        task->next = task_list_head->next;
        task_list_head->next = task;
    }
    active_tasks++;
}

uint32_t task_create(task_entry_t entry)
{
    task_t *task = (task_t *)kmalloc(sizeof(task_t));
    if (!task)
        return 0;

    kmemset(task, 0, sizeof(task_t));

    void *stack = kmalloc(TASK_STACK_SIZE);
    if (!stack) {
        kfree(task);
        return 0;
    }
    kmemset(stack, 0, TASK_STACK_SIZE);

    task->id         = next_task_id++;
    task->state      = TASK_READY;
    task->stack_base = (uint32_t)stack;
    task->stack_top  = (uint32_t)stack + TASK_STACK_SIZE;

    /* Set up stack to match what context_switch expects to pop.
     *
     * context_switch saves:  pushf, push edi, push esi, push ebx, push ebp
     * context_switch restores: pop ebp, pop ebx, pop esi, pop edi, popf, ret
     *
     * Stack layout (high addr -> low addr, sp decrements):
     *   [ret_addr]  task_trampoline
     *   [EFLAGS]    0x202 (IF=1)
     *   [EDI]       0
     *   [ESI]       0
     *   [EBX]       entry point (trampoline calls *EBX)
     *   [EBP]       0               <- ESP points here
     */
    uint32_t *sp = (uint32_t *)task->stack_top;

    *(--sp) = (uint32_t)task_trampoline;    /* ret address (popped by ret) */
    *(--sp) = 0x202;                        /* EFLAGS (popped by popf) */
    *(--sp) = 0;                            /* EDI */
    *(--sp) = 0;                            /* ESI */
    *(--sp) = (uint32_t)entry;              /* EBX = entry point */
    *(--sp) = 0;                            /* EBP */

    task->context.esp = (uint32_t)sp;

    task_list_add(task);
    return task->id;
}

void task_exit(void)
{
    if (current_task) {
        current_task->state = TASK_DEAD;
        schedule();
    }
    while (1) __asm__ volatile ("hlt");
}

void scheduler_init(void)
{
    /* Wrap the current execution context as "main" task */
    task_t *main_task = (task_t *)kmalloc(sizeof(task_t));
    kmemset(main_task, 0, sizeof(task_t));

    main_task->id         = next_task_id++;
    main_task->state      = TASK_RUNNING;
    main_task->stack_base = 0;
    main_task->stack_top  = 0;

    current_task = main_task;
    task_list_add(main_task);

    /* Create idle task */
    task_create(idle_task_entry);

    /* Replace PIT handler with scheduler-aware version */
    isr_register_handler(32, scheduler_tick);

    scheduler_active = true;
}
