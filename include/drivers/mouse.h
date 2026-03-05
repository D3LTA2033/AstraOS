/* ==========================================================================
 * AstraOS - PS/2 Mouse Driver
 * ==========================================================================
 * Handles IRQ12 for the PS/2 mouse. Tracks cursor position and button
 * state. Provides a simple polling interface.
 * ========================================================================== */

#ifndef _ASTRA_DRIVERS_MOUSE_H
#define _ASTRA_DRIVERS_MOUSE_H

#include <kernel/types.h>

#define MOUSE_LEFT   0x01
#define MOUSE_RIGHT  0x02
#define MOUSE_MIDDLE 0x04

typedef struct {
    int32_t  x, y;          /* Current position */
    uint8_t  buttons;       /* Button state bitmask */
    bool     moved;         /* Set when position changed */
    bool     clicked;       /* Set when button pressed (edge) */
    bool     released;      /* Set when button released (edge) */
    uint8_t  click_btn;     /* Which button was clicked */
    uint8_t  release_btn;   /* Which button was released */
} mouse_state_t;

/* Initialize the PS/2 mouse */
void mouse_init(uint32_t screen_w, uint32_t screen_h);

/* Get current mouse state (and clear event flags) */
mouse_state_t mouse_get_state(void);

/* Peek at state without clearing flags */
mouse_state_t mouse_peek_state(void);

/* Set mouse position bounds */
void mouse_set_bounds(uint32_t w, uint32_t h);

#endif /* _ASTRA_DRIVERS_MOUSE_H */
