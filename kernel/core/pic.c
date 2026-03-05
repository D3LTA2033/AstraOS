/* ==========================================================================
 * AstraOS - 8259 PIC Implementation
 * ==========================================================================
 * Remaps PIC1 to vectors 0x20-0x27 and PIC2 to 0x28-0x2F.
 * After init, all IRQs are masked except the cascade line (IRQ2).
 * Individual drivers unmask their IRQ when they register.
 * ========================================================================== */

#include <kernel/pic.h>
#include <io.h>

/* PIC I/O ports */
#define PIC1_CMD    0x20
#define PIC1_DATA   0x21
#define PIC2_CMD    0xA0
#define PIC2_DATA   0xA1

/* ICW (Initialization Command Words) */
#define ICW1_INIT   0x10    /* Initialization bit */
#define ICW1_ICW4   0x01    /* ICW4 needed */
#define ICW4_8086   0x01    /* 8086/88 mode */

void pic_init(void)
{
    /* Save current masks */
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);
    (void)mask1;
    (void)mask2;

    /* ICW1: Start initialization sequence (cascade mode, ICW4 needed) */
    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);
    io_wait();

    /* ICW2: Set vector offsets */
    outb(PIC1_DATA, PIC1_OFFSET);      /* Master: IRQ 0-7  -> 0x20-0x27 */
    io_wait();
    outb(PIC2_DATA, PIC2_OFFSET);      /* Slave:  IRQ 8-15 -> 0x28-0x2F */
    io_wait();

    /* ICW3: Tell master about slave on IRQ2, tell slave its cascade identity */
    outb(PIC1_DATA, 0x04);             /* Master: slave on IRQ2 (bit 2) */
    io_wait();
    outb(PIC2_DATA, 0x02);             /* Slave: cascade identity = 2 */
    io_wait();

    /* ICW4: Set 8086 mode */
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    /* Mask all IRQs except cascade (IRQ2) — drivers will unmask as needed */
    outb(PIC1_DATA, 0xFB);             /* 1111 1011 = all masked except IRQ2 */
    outb(PIC2_DATA, 0xFF);             /* 1111 1111 = all masked */
}

void pic_send_eoi(uint8_t irq)
{
    /* If IRQ came from PIC2 (8-15), send EOI to both */
    if (irq >= 8)
        outb(PIC2_CMD, 0x20);
    outb(PIC1_CMD, 0x20);
}

void pic_set_mask(uint8_t irq)
{
    uint16_t port;
    uint8_t  val;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    val = inb(port) | (1 << irq);
    outb(port, val);
}

void pic_clear_mask(uint8_t irq)
{
    uint16_t port;
    uint8_t  val;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    val = inb(port) & ~(1 << irq);
    outb(port, val);
}
