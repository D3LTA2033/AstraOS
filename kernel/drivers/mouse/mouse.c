/* ==========================================================================
 * AstraOS - PS/2 Mouse Driver
 * ==========================================================================
 * Handles IRQ12. The PS/2 mouse sends 3-byte packets:
 *   Byte 0: [Y overflow | X overflow | Y sign | X sign | 1 | Middle | Right | Left]
 *   Byte 1: X movement (delta)
 *   Byte 2: Y movement (delta)
 * ========================================================================== */

#include <drivers/mouse.h>
#include <kernel/idt.h>
#include <kernel/pic.h>
#include <io.h>

#define PS2_DATA    0x60
#define PS2_STATUS  0x64
#define PS2_CMD     0x64

static volatile mouse_state_t state;
static volatile uint8_t packet[3];
static volatile int     packet_idx = 0;
static uint32_t max_x = 1024, max_y = 768;

static void ps2_wait_input(void)
{
    int timeout = 100000;
    while (timeout--) {
        if (inb(PS2_STATUS) & 0x02) continue;
        return;
    }
}

static void ps2_wait_output(void)
{
    int timeout = 100000;
    while (timeout--) {
        if (inb(PS2_STATUS) & 0x01) return;
    }
}

static void ps2_write(uint8_t val)
{
    ps2_wait_input();
    outb(PS2_CMD, 0xD4);  /* Write to mouse port */
    ps2_wait_input();
    outb(PS2_DATA, val);
}

static uint8_t ps2_read(void)
{
    ps2_wait_output();
    return inb(PS2_DATA);
}

static void mouse_callback(registers_t *regs)
{
    (void)regs;
    uint8_t data = inb(PS2_DATA);

    if (packet_idx == 0 && !(data & 0x08)) {
        /* Byte 0 must have bit 3 set — resync */
        pic_send_eoi(12);
        return;
    }

    packet[packet_idx++] = data;

    if (packet_idx >= 3) {
        packet_idx = 0;

        uint8_t flags = packet[0];
        int16_t dx = (int16_t)packet[1];
        int16_t dy = (int16_t)packet[2];

        /* Apply sign extension */
        if (flags & 0x10) dx |= (int16_t)0xFF00;
        if (flags & 0x20) dy |= (int16_t)0xFF00;

        /* Discard overflow packets */
        if (flags & 0xC0) {
            pic_send_eoi(12);
            return;
        }

        /* Update position — Y is inverted (mouse up = negative delta) */
        int32_t nx = state.x + dx;
        int32_t ny = state.y - dy;
        if (nx < 0) nx = 0;
        if (ny < 0) ny = 0;
        if ((uint32_t)nx >= max_x) nx = (int32_t)(max_x - 1);
        if ((uint32_t)ny >= max_y) ny = (int32_t)(max_y - 1);

        if (nx != state.x || ny != state.y)
            state.moved = true;
        state.x = nx;
        state.y = ny;

        /* Check button changes */
        uint8_t new_btn = flags & 0x07;
        uint8_t pressed  = new_btn & ~state.buttons;
        uint8_t released = state.buttons & ~new_btn;

        if (pressed) {
            state.clicked = true;
            state.click_btn = pressed;
        }
        if (released) {
            state.released = true;
            state.release_btn = released;
        }
        state.buttons = new_btn;
    }

    pic_send_eoi(12);
}

mouse_state_t mouse_get_state(void)
{
    mouse_state_t s = state;
    state.moved = false;
    state.clicked = false;
    state.released = false;
    state.click_btn = 0;
    state.release_btn = 0;
    return s;
}

mouse_state_t mouse_peek_state(void)
{
    return state;
}

void mouse_set_bounds(uint32_t w, uint32_t h)
{
    max_x = w;
    max_y = h;
}

void mouse_init(uint32_t screen_w, uint32_t screen_h)
{
    max_x = screen_w;
    max_y = screen_h;
    state.x = (int32_t)(screen_w / 2);
    state.y = (int32_t)(screen_h / 2);
    state.buttons = 0;

    /* Enable the auxiliary mouse device */
    ps2_wait_input();
    outb(PS2_CMD, 0xA8);   /* Enable aux port */

    /* Enable IRQ12 */
    ps2_wait_input();
    outb(PS2_CMD, 0x20);   /* Read controller config */
    uint8_t config = ps2_read();
    config |= 0x02;        /* Enable IRQ12 */
    config &= ~0x20;       /* Clear disable mouse clock bit */
    ps2_wait_input();
    outb(PS2_CMD, 0x60);   /* Write controller config */
    ps2_wait_input();
    outb(PS2_DATA, config);

    /* Reset mouse */
    ps2_write(0xFF);
    ps2_read(); /* ACK */
    ps2_read(); /* Self-test result */
    ps2_read(); /* Mouse ID */

    /* Use default settings */
    ps2_write(0xF6);
    ps2_read(); /* ACK */

    /* Enable data reporting */
    ps2_write(0xF4);
    ps2_read(); /* ACK */

    /* Register IRQ handler */
    isr_register_handler(44, mouse_callback);  /* IRQ12 = ISR 44 */
    pic_clear_mask(12);
}
