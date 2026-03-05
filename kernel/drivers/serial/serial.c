/* ==========================================================================
 * AstraOS - Serial Port Driver (8250/16550 UART)
 * ==========================================================================
 * Configures COM1 at 115200 baud, 8 data bits, no parity, 1 stop bit.
 * Registers as /dev/serial0 via devfs.
 *
 * Register map (base + offset):
 *   +0  Data / Divisor Latch Low (DLAB=0/1)
 *   +1  Interrupt Enable / Divisor Latch High
 *   +2  FIFO Control / Interrupt ID
 *   +3  Line Control (data bits, stop, parity, DLAB)
 *   +4  Modem Control (DTR, RTS, OUT2 for interrupts)
 *   +5  Line Status (data ready, transmit empty, errors)
 * ========================================================================== */

#include <drivers/serial.h>
#include <kernel/devfs.h>
#include <kernel/vfs.h>
#include <kernel/kstring.h>

/* Port I/O — architecture-specific, inlined from arch headers */
static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

#define COM1 SERIAL_COM1_BASE

void serial_init(void)
{
    /* Disable interrupts */
    outb(COM1 + 1, 0x00);

    /* Enable DLAB (set baud rate divisor) */
    outb(COM1 + 3, 0x80);

    /* Set divisor to 1 (115200 baud) — low byte then high byte */
    outb(COM1 + 0, 0x01);
    outb(COM1 + 1, 0x00);

    /* 8 bits, no parity, 1 stop bit (8N1), clear DLAB */
    outb(COM1 + 3, 0x03);

    /* Enable FIFO, clear them, 14-byte threshold */
    outb(COM1 + 2, 0xC7);

    /* Set RTS/DSR, enable OUT2 (required for interrupts on PC) */
    outb(COM1 + 4, 0x0B);

    /* Loopback test — verify the UART is functional */
    outb(COM1 + 4, 0x1E);      /* Enable loopback mode */
    outb(COM1 + 0, 0xAE);      /* Send test byte */
    if (inb(COM1 + 0) != 0xAE) {
        /* UART is faulty — proceed anyway, output may not work */
    }

    /* Disable loopback, restore normal operation */
    outb(COM1 + 4, 0x0F);
}

static int serial_transmit_ready(void)
{
    return inb(COM1 + 5) & 0x20;
}

void serial_putchar(char c)
{
    while (!serial_transmit_ready())
        ;
    outb(COM1, (uint8_t)c);
}

void serial_write(const char *str)
{
    while (*str) {
        if (*str == '\n')
            serial_putchar('\r');
        serial_putchar(*str++);
    }
}

int serial_data_available(void)
{
    return inb(COM1 + 5) & 0x01;
}

int serial_read_byte(void)
{
    if (!serial_data_available())
        return -1;
    return inb(COM1);
}

/* --- VFS device node operations --- */

static uint32_t serial_vfs_write(vfs_node_t *node, uint32_t offset,
                                 uint32_t size, const uint8_t *buffer)
{
    (void)node;
    (void)offset;
    for (uint32_t i = 0; i < size; i++) {
        if (buffer[i] == '\n')
            serial_putchar('\r');
        serial_putchar((char)buffer[i]);
    }
    return size;
}

static uint32_t serial_vfs_read(vfs_node_t *node, uint32_t offset,
                                uint32_t size, uint8_t *buffer)
{
    (void)node;
    (void)offset;
    uint32_t count = 0;
    while (count < size && serial_data_available()) {
        int b = serial_read_byte();
        if (b < 0)
            break;
        buffer[count++] = (uint8_t)b;
    }
    return count;
}

void serial_register_device(void)
{
    devfs_register_chardev("serial0",
                           serial_vfs_read,
                           serial_vfs_write,
                           NULL,    /* open — no special setup */
                           NULL);   /* close — no special teardown */
}
