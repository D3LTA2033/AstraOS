/* ==========================================================================
 * AstraOS - PS/2 Keyboard Driver
 * ==========================================================================
 * Scancode set 1 keyboard driver with ring buffer. Characters are buffered
 * by the IRQ handler and consumed by keyboard_getchar().
 * ========================================================================== */

#ifndef _ASTRA_DRIVERS_KEYBOARD_H
#define _ASTRA_DRIVERS_KEYBOARD_H

#include <kernel/types.h>

/* Initialize keyboard driver and register IRQ1 handler */
void keyboard_init(void);

/* Get next character from keyboard buffer. Returns -1 if empty. */
int keyboard_getchar(void);

/* Check if keyboard data is available */
int keyboard_has_data(void);

#endif /* _ASTRA_DRIVERS_KEYBOARD_H */
