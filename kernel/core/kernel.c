/* ==========================================================================
 * AstraOS - Kernel Entry Point
 * ==========================================================================
 * Phase 5: Adds user mode (ring 3), system calls, and user-space tasks.
 * ========================================================================== */

#include <kernel/types.h>
#include <kernel/multiboot.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/pic.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <kernel/page_fault.h>
#include <kernel/kheap.h>
#include <kernel/task.h>
#include <kernel/scheduler.h>
#include <kernel/syscall.h>
#include <kernel/usermode.h>
#include <drivers/vga.h>
#include <drivers/pit.h>
#include <drivers/keyboard.h>

/* Embedded user program (linked as binary blob via ld -b binary) */
extern char _binary_user_test_user_bin_start[];
extern char _binary_user_test_user_bin_end[];

static void print_dec(uint32_t val)
{
    if (val == 0) { vga_putchar('0'); return; }
    char buf[12];
    int i = 0;
    while (val > 0) { buf[i++] = '0' + (char)(val % 10); val /= 10; }
    for (int j = i - 1; j >= 0; j--) vga_putchar(buf[j]);
}

void kernel_main(uint32_t magic, uint32_t mboot_addr)
{
    multiboot_info_t *mboot = (multiboot_info_t *)mboot_addr;

    vga_init();

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        vga_set_color(vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
        vga_write("FATAL: Not loaded by a Multiboot bootloader!\n");
        return;
    }

    /* Boot banner */
    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    vga_write("========================================\n");
    vga_write("  AstraOS v0.5.0 - AstraCore Kernel\n");
    vga_write("  Architecture: x86 (i686)\n");
    vga_write("  Phase 5: User Mode + Syscalls\n");
    vga_write("========================================\n\n");

    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));

    /* Phase 2 */
    gdt_init();
    vga_write("[OK] GDT initialized\n");
    idt_init();
    vga_write("[OK] IDT initialized\n");
    pic_init();
    vga_write("[OK] PIC remapped\n");
    pit_init();
    vga_write("[OK] PIT timer at 100 Hz\n");
    keyboard_init();
    vga_write("[OK] Keyboard driver loaded\n");
    pmm_init(mboot);

    /* Phase 3 */
    page_fault_init();
    vmm_init();
    kheap_init();
    vga_write("[OK] Kernel heap initialized\n");

    /* Phase 4 */
    scheduler_init();
    vga_write("[OK] Scheduler initialized\n");

    /* Phase 5 */
    syscall_init();
    vga_write("[OK] Syscall interface (int 0x80) registered\n");

    usermode_init();
    vga_write("[OK] User mode support initialized\n");

    /* Spawn user-space task */
    size_t user_size = (size_t)(_binary_user_test_user_bin_end -
                                _binary_user_test_user_bin_start);
    vga_write("[OK] User program: ");
    print_dec((uint32_t)user_size);
    vga_write(" bytes\n");

    uint32_t utid = usermode_task_create(
        _binary_user_test_user_bin_start, user_size);

    if (utid) {
        vga_set_color(vga_entry_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));
        vga_write("[*] User task spawned (id=");
        print_dec(utid);
        vga_write(") -> ring 3\n\n");
    } else {
        vga_set_color(vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
        vga_write("[FAIL] Could not create user task\n");
    }

    /* Enable interrupts */
    __asm__ volatile ("sti");

    /* Wait for user task to finish */
    uint32_t start = pit_get_ticks();
    while (pit_get_ticks() - start < 400)
        __asm__ volatile ("hlt");

    vga_set_color(vga_entry_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));
    vga_write("\n[*] Active tasks: ");
    print_dec(sched_task_count());
    vga_write(". Keyboard active.\n");
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    vga_write("Type something: ");

    while (1) {
        __asm__ volatile ("hlt");
    }
}
