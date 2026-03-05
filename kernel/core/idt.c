/* ==========================================================================
 * AstraOS - IDT Implementation
 * ==========================================================================
 * Sets up 256 IDT entries. Vectors 0-31 are CPU exceptions (ISRs),
 * vectors 32-47 are hardware IRQs. The common C handler dispatches
 * to registered handler functions.
 * ========================================================================== */

#include <kernel/idt.h>
#include <kernel/gdt.h>
#include <kernel/kstring.h>
#include <drivers/vga.h>

static idt_entry_t idt_entries[IDT_ENTRIES];
static idt_ptr_t   idt_ptr;

/* Handler table — one slot per interrupt vector */
static isr_handler_t interrupt_handlers[IDT_ENTRIES];

/* Exception names for debugging */
static const char *exception_names[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point",
    "Virtualization",
    "Control Protection",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved",
    "Hypervisor Injection",
    "VMM Communication",
    "Security Exception",
    "Reserved",
};

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags)
{
    idt_entries[num].base_low  = (uint16_t)(base & 0xFFFF);
    idt_entries[num].base_high = (uint16_t)((base >> 16) & 0xFFFF);
    idt_entries[num].selector  = sel;
    idt_entries[num].zero      = 0;
    idt_entries[num].flags     = flags;
}

void isr_register_handler(uint8_t n, isr_handler_t handler)
{
    interrupt_handlers[n] = handler;
}

/* Called from assembly stubs — the common C dispatcher */
void isr_handler(registers_t *regs)
{
    if (interrupt_handlers[regs->int_no]) {
        interrupt_handlers[regs->int_no](regs);
    } else if (regs->int_no < 32) {
        /* Unhandled CPU exception — panic */
        vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_RED));
        vga_write("\n KERNEL PANIC: ");
        vga_write(exception_names[regs->int_no]);
        vga_write("\n");
        vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));

        /* Halt */
        __asm__ volatile ("cli; hlt");
    }
}

void idt_init(void)
{
    idt_ptr.limit = (sizeof(idt_entry_t) * IDT_ENTRIES) - 1;
    idt_ptr.base  = (uint32_t)&idt_entries;

    kmemset(&idt_entries, 0, sizeof(idt_entries));
    kmemset(&interrupt_handlers, 0, sizeof(interrupt_handlers));

    /* CPU exceptions (ISRs 0-31) */
    idt_set_gate(0,  (uint32_t)isr0,  GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(1,  (uint32_t)isr1,  GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(2,  (uint32_t)isr2,  GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(3,  (uint32_t)isr3,  GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(4,  (uint32_t)isr4,  GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(5,  (uint32_t)isr5,  GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(6,  (uint32_t)isr6,  GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(7,  (uint32_t)isr7,  GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(8,  (uint32_t)isr8,  GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(9,  (uint32_t)isr9,  GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(10, (uint32_t)isr10, GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(11, (uint32_t)isr11, GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(12, (uint32_t)isr12, GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(13, (uint32_t)isr13, GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(14, (uint32_t)isr14, GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(15, (uint32_t)isr15, GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(16, (uint32_t)isr16, GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(17, (uint32_t)isr17, GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(18, (uint32_t)isr18, GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(19, (uint32_t)isr19, GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(20, (uint32_t)isr20, GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(21, (uint32_t)isr21, GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(22, (uint32_t)isr22, GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(23, (uint32_t)isr23, GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(24, (uint32_t)isr24, GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(25, (uint32_t)isr25, GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(26, (uint32_t)isr26, GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(27, (uint32_t)isr27, GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(28, (uint32_t)isr28, GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(29, (uint32_t)isr29, GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(30, (uint32_t)isr30, GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(31, (uint32_t)isr31, GDT_KERNEL_CODE, IDT_GATE_INT32);

    /* Hardware IRQs (32-47) */
    idt_set_gate(32, (uint32_t)irq0,  GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(33, (uint32_t)irq1,  GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(34, (uint32_t)irq2,  GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(35, (uint32_t)irq3,  GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(36, (uint32_t)irq4,  GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(37, (uint32_t)irq5,  GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(38, (uint32_t)irq6,  GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(39, (uint32_t)irq7,  GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(40, (uint32_t)irq8,  GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(41, (uint32_t)irq9,  GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(42, (uint32_t)irq10, GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(43, (uint32_t)irq11, GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(44, (uint32_t)irq12, GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(45, (uint32_t)irq13, GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(46, (uint32_t)irq14, GDT_KERNEL_CODE, IDT_GATE_INT32);
    idt_set_gate(47, (uint32_t)irq15, GDT_KERNEL_CODE, IDT_GATE_INT32);

    idt_flush((uint32_t)&idt_ptr);
}
