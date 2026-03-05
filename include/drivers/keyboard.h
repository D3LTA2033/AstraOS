/* ==========================================================================
 * AstraOS - PS/2 Keyboard Driver
 * ==========================================================================
 * Basic scancode set 1 keyboard driver. Reads from port 0x60 on IRQ1.
 * Translates scan codes to ASCII for printable keys.
 * ========================================================================== */

#ifndef _ASTRA_DRIVERS_KEYBOARD_H
#define _ASTRA_DRIVERS_KEYBOARD_H

#include <kernel/types.h>

/* Initialize keyboard driver and register IRQ1 handler */
void keyboard_init(void);

#endif /* _ASTRA_DRIVERS_KEYBOARD_H */
