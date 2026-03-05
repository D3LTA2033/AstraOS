/* ==========================================================================
 * AstraOS - Serial Port Driver (8250/16550 UART)
 * ==========================================================================
 * Drives COM1 (0x3F8) for kernel debug output and as a character device.
 * The serial port is configured at 115200 baud, 8N1.
 *
 * After initialization, the serial port is registered as /dev/serial0
 * via devfs, making it accessible through the VFS.
 * ========================================================================== */

#ifndef _ASTRA_DRIVERS_SERIAL_H
#define _ASTRA_DRIVERS_SERIAL_H

#include <kernel/types.h>

/* COM1 base I/O port */
#define SERIAL_COM1_BASE    0x3F8

/* Initialize COM1 at 115200 baud, 8N1 */
void serial_init(void);

/* Write a single byte to COM1 (blocks until transmit buffer is empty) */
void serial_putchar(char c);

/* Write a null-terminated string to COM1 */
void serial_write(const char *str);

/* Read a byte from COM1 (returns -1 if no data available) */
int serial_read_byte(void);

/* Check if data is available to read */
int serial_data_available(void);

/* Register serial port as /dev/serial0 in devfs */
void serial_register_device(void);

#endif /* _ASTRA_DRIVERS_SERIAL_H */
