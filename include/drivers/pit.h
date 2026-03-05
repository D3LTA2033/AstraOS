/* ==========================================================================
 * AstraOS - Programmable Interval Timer (PIT 8253/8254)
 * ==========================================================================
 * Channel 0 generates periodic IRQ0 interrupts. We configure it at ~100Hz
 * (10ms tick) for the kernel timer. The tick counter is the foundation for
 * all time-based operations (sleep, scheduler quantum, etc.).
 * ========================================================================== */

#ifndef _ASTRA_DRIVERS_PIT_H
#define _ASTRA_DRIVERS_PIT_H

#include <kernel/types.h>

#define PIT_FREQUENCY 100   /* Hz — 10ms per tick */

/* Initialize PIT channel 0 and register IRQ0 handler */
void pit_init(void);

/* Get current tick count since boot */
uint32_t pit_get_ticks(void);

/* Increment tick counter (called by scheduler's timer handler) */
void pit_tick(void);

#endif /* _ASTRA_DRIVERS_PIT_H */
