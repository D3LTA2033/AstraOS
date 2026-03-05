/* ==========================================================================
 * AstraOS - PS/2 Keyboard Driver
 * ==========================================================================
 * Handles IRQ1 (keyboard interrupt). Reads scan codes from port 0x60
 * and translates them to ASCII using a US QWERTY scancode set 1 table.
 * Key releases (bit 7 set) are ignored for now.
 * ========================================================================== */

#include <drivers/keyboard.h>
#include <drivers/vga.h>
#include <kernel/idt.h>
#include <kernel/pic.h>
#include <io.h>

#define KB_DATA_PORT 0x60

/* US QWERTY scancode set 1 -> ASCII (lowercase only, index = scancode) */
static const char scancode_to_ascii[128] = {
    0,   27, '1', '2', '3', '4', '5', '6', '7', '8',  /* 0x00-0x09 */
    '9', '0', '-', '=', '\b','\t', 'q', 'w', 'e', 'r', /* 0x0A-0x13 */
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,   /* 0x14-0x1D (0x1D = LCtrl) */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',  /* 0x1E-0x27 */
    '\'','`',  0,  '\\','z', 'x', 'c', 'v', 'b', 'n',   /* 0x28-0x31 */
    'm', ',', '.', '/',  0,  '*',  0,  ' ',  0,   0,     /* 0x32-0x3B */
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,      /* 0x3C-0x45 (F1-F10) */
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,      /* 0x46-0x4F */
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,      /* 0x50-0x59 */
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,      /* 0x5A-0x63 */
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,      /* 0x64-0x6D */
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,      /* 0x6E-0x77 */
    0,   0,   0,   0,   0,   0,   0,   0                  /* 0x78-0x7F */
};

static void keyboard_callback(registers_t *regs)
{
    (void)regs;
    uint8_t scancode = inb(KB_DATA_PORT);

    /* Ignore key releases (bit 7 set) */
    if (scancode & 0x80) {
        pic_send_eoi(1);
        return;
    }

    char c = scancode_to_ascii[scancode];
    if (c) {
        vga_putchar(c);
    }

    pic_send_eoi(1);
}

void keyboard_init(void)
{
    isr_register_handler(33, keyboard_callback);
    pic_clear_mask(1);
}
