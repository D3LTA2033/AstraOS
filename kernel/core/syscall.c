/* ==========================================================================
 * AstraOS - System Call Implementation
 * ==========================================================================
 * Handles int 0x80 from user space. The ISR stub pushes all registers,
 * and we extract syscall number from EAX, args from EBX-EDI.
 *
 * Syscall dispatch table maps numbers to handler functions.
 * ========================================================================== */

#include <kernel/syscall.h>
#include <kernel/idt.h>
#include <kernel/gdt.h>
#include <kernel/task.h>
#include <kernel/scheduler.h>
#include <drivers/vga.h>
#include <drivers/pit.h>

/* --- Syscall implementations --- */

static uint32_t sys_exit(uint32_t code, uint32_t b, uint32_t c,
                         uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e; (void)code;
    task_exit();
    return 0;   /* Never reached */
}

static uint32_t sys_write(uint32_t fd, uint32_t buf_addr, uint32_t len,
                          uint32_t d, uint32_t e)
{
    (void)d; (void)e;

    /* For now, fd=1 is stdout (VGA console). Others are unsupported. */
    if (fd != 1)
        return (uint32_t)-1;

    /* Basic bounds check — buffer must be in user-space range (< 0xC0000000) */
    if (buf_addr >= 0xC0000000 || buf_addr + len >= 0xC0000000)
        return (uint32_t)-1;

    const char *buf = (const char *)buf_addr;
    for (uint32_t i = 0; i < len; i++)
        vga_putchar(buf[i]);

    return len;
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

/* --- Dispatch table --- */

static syscall_fn_t syscall_table[SYSCALL_COUNT] = {
    [SYS_EXIT]    = sys_exit,
    [SYS_WRITE]   = sys_write,
    [SYS_GETPID]  = sys_getpid,
    [SYS_YIELD]   = sys_yield,
    [SYS_SLEEP]   = sys_sleep,
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
    /* Register int 0x80 as a user-callable interrupt gate (DPL=3) */
    idt_set_gate(SYSCALL_VECTOR, (uint32_t)isr128, GDT_KERNEL_CODE, IDT_GATE_USER);
    isr_register_handler(SYSCALL_VECTOR, syscall_handler);
}
