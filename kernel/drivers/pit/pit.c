/* ==========================================================================
 * AstraOS - PIT Timer Driver
 * ==========================================================================
 * Configures PIT channel 0 as a periodic timer at PIT_FREQUENCY Hz.
 * The base PIT oscillator runs at 1,193,182 Hz.
 * Divisor = 1193182 / desired_freq.
 * ========================================================================== */

#include <drivers/pit.h>
#include <kernel/idt.h>
#include <kernel/pic.h>
#include <io.h>

/* PIT I/O ports */
#define PIT_CHANNEL0 0x40
#define PIT_CMD      0x43

/* PIT base frequency */
#define PIT_BASE_FREQ 1193182

static volatile uint32_t tick_count = 0;

static void pit_callback(registers_t *regs)
{
    (void)regs;
    tick_count++;
    pic_send_eoi(0);
}

void pit_tick(void)
{
    tick_count++;
}

uint32_t pit_get_ticks(void)
{
    return tick_count;
}

void pit_init(void)
{
    uint16_t divisor = (uint16_t)(PIT_BASE_FREQ / PIT_FREQUENCY);

    /* Channel 0, access mode lo/hi byte, mode 3 (square wave), binary */
    outb(PIT_CMD, 0x36);

    /* Send divisor (low byte first, then high byte) */
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    /* Register handler and unmask IRQ0 */
    isr_register_handler(32, pit_callback);
    pic_clear_mask(0);
}
