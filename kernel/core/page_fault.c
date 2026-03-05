/* ==========================================================================
 * AstraOS - Page Fault Handler (ISR 14)
 * ==========================================================================
 * Decodes the page fault error code and CR2 (faulting address).
 * For now, all page faults are fatal (kernel panic).
 * In later phases, this will handle:
 *   - Demand paging / lazy allocation
 *   - Copy-on-write (COW)
 *   - Swapping
 * ========================================================================== */

#include <kernel/idt.h>
#include <kernel/vmm.h>
#include <drivers/vga.h>

/* Page fault error code bits */
#define PF_PRESENT   (1 << 0)  /* 0 = not present, 1 = protection violation */
#define PF_WRITE     (1 << 1)  /* 0 = read, 1 = write */
#define PF_USER      (1 << 2)  /* 0 = kernel, 1 = user mode */
#define PF_RESERVED  (1 << 3)  /* 1 = reserved bit set in page entry */
#define PF_IFETCH    (1 << 4)  /* 1 = instruction fetch */

static void print_hex(uint32_t val)
{
    const char hex[] = "0123456789ABCDEF";
    char buf[11] = "0x00000000";
    for (int i = 9; i >= 2; i--) {
        buf[i] = hex[val & 0xF];
        val >>= 4;
    }
    vga_write(buf);
}

static void page_fault_handler(registers_t *regs)
{
    uint32_t faulting_addr = paging_get_cr2();
    uint32_t err = regs->err_code;

    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_RED));
    vga_write("\n KERNEL PANIC: Page Fault!\n");

    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    vga_write(" Address: ");
    print_hex(faulting_addr);
    vga_write("\n");

    vga_write(" Reason:  ");
    if (!(err & PF_PRESENT))
        vga_write("page not present");
    else
        vga_write("protection violation");

    vga_write(err & PF_WRITE ? " (write)" : " (read)");
    vga_write(err & PF_USER ? " [user-mode]" : " [kernel-mode]");
    if (err & PF_RESERVED)
        vga_write(" [reserved-bit]");
    if (err & PF_IFETCH)
        vga_write(" [instruction-fetch]");
    vga_write("\n");

    vga_write(" EIP:     ");
    print_hex(regs->eip);
    vga_write("\n");

    /* Halt — unrecoverable in Phase 3 */
    __asm__ volatile ("cli; hlt");
}

void page_fault_init(void)
{
    isr_register_handler(14, page_fault_handler);
}
