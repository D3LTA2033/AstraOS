/* ==========================================================================
 * AstraOS - PS/2 Keyboard Driver
 * ==========================================================================
 * Handles IRQ1. Reads scan codes from port 0x60 and translates them to
 * ASCII using US QWERTY scancode set 1. Characters are placed in a ring
 * buffer for consumption by the console line discipline.
 *
 * Phase 8: Added ring buffer, shift/capslock support, backspace handling.
 * The driver no longer echoes to VGA directly — that is the console's job.
 * ========================================================================== */

#include <drivers/keyboard.h>
#include <kernel/idt.h>
#include <kernel/pic.h>
#include <io.h>

#define KB_DATA_PORT    0x60
#define KB_BUF_SIZE     256

/* Ring buffer */
static volatile char     kb_buffer[KB_BUF_SIZE];
static volatile uint32_t kb_write = 0;
static volatile uint32_t kb_read  = 0;

/* Modifier state */
static bool shift_held = false;
static bool caps_on    = false;

/* Scancode set 1 -> ASCII (unshifted) */
static const char sc_normal[128] = {
    0,   27, '1', '2', '3', '4', '5', '6', '7', '8',
    '9', '0', '-', '=', '\b','\t', 'q', 'w', 'e', 'r',
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'','`',  0, '\\', 'z', 'x', 'c', 'v', 'b', 'n',
    'm', ',', '.', '/',  0,  '*',  0,  ' ',  0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0
};

/* Scancode set 1 -> ASCII (shifted) */
static const char sc_shift[128] = {
    0,   27, '!', '@', '#', '$', '%', '^', '&', '*',
    '(', ')', '_', '+', '\b','\t', 'Q', 'W', 'E', 'R',
    'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"', '~',  0,  '|', 'Z', 'X', 'C', 'V', 'B', 'N',
    'M', '<', '>', '?',  0,  '*',  0,  ' ',  0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0
};

static void kb_buf_put(char c)
{
    uint32_t next = (kb_write + 1) % KB_BUF_SIZE;
    if (next != kb_read) {
        kb_buffer[kb_write] = c;
        kb_write = next;
    }
}

int keyboard_getchar(void)
{
    if (kb_read == kb_write)
        return -1;
    char c = kb_buffer[kb_read];
    kb_read = (kb_read + 1) % KB_BUF_SIZE;
    return (int)(unsigned char)c;
}

int keyboard_has_data(void)
{
    return kb_read != kb_write;
}

static void keyboard_callback(registers_t *regs)
{
    (void)regs;
    uint8_t scancode = inb(KB_DATA_PORT);

    /* Left/Right Shift press/release */
    if (scancode == 0x2A || scancode == 0x36) {
        shift_held = true;
        pic_send_eoi(1);
        return;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_held = false;
        pic_send_eoi(1);
        return;
    }

    /* Caps Lock toggle */
    if (scancode == 0x3A) {
        caps_on = !caps_on;
        pic_send_eoi(1);
        return;
    }

    /* Ignore key releases */
    if (scancode & 0x80) {
        pic_send_eoi(1);
        return;
    }

    /* Ignore extended prefix */
    if (scancode == 0xE0) {
        pic_send_eoi(1);
        return;
    }

    /* Translate scancode to ASCII */
    char c;
    if (shift_held)
        c = sc_shift[scancode & 0x7F];
    else
        c = sc_normal[scancode & 0x7F];

    /* Apply caps lock to letters */
    if (caps_on) {
        if (c >= 'a' && c <= 'z')
            c -= 32;
        else if (c >= 'A' && c <= 'Z')
            c += 32;
    }

    if (c)
        kb_buf_put(c);

    pic_send_eoi(1);
}

void keyboard_init(void)
{
    isr_register_handler(33, keyboard_callback);
    pic_clear_mask(1);
}
