/* ==========================================================================
 * AstraOS - VGA Text Mode Driver
 * ==========================================================================
 * Drives the standard 80x25 VGA text buffer at 0xB8000.
 * Supports scrolling, newlines, and cursor tracking.
 * ========================================================================== */

#include <drivers/vga.h>
#include <kernel/kstring.h>

/* VGA text buffer — mapped at fixed physical address */
static uint16_t *const VGA_BUFFER = (uint16_t *)0xB8000;

/* Driver state */
static size_t  vga_row;
static size_t  vga_col;
static uint8_t vga_attr;

/* Scroll the screen up by one line */
static void vga_scroll(void)
{
    /* Move all rows up by one */
    kmemcpy(VGA_BUFFER,
            VGA_BUFFER + VGA_WIDTH,
            sizeof(uint16_t) * VGA_WIDTH * (VGA_HEIGHT - 1));

    /* Clear the last row */
    for (size_t x = 0; x < VGA_WIDTH; x++)
        VGA_BUFFER[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', vga_attr);
}

void vga_init(void)
{
    vga_row  = 0;
    vga_col  = 0;
    vga_attr = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_clear();
}

void vga_set_color(uint8_t color)
{
    vga_attr = color;
}

void vga_clear(void)
{
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_BUFFER[y * VGA_WIDTH + x] = vga_entry(' ', vga_attr);

    vga_row = 0;
    vga_col = 0;
}

void vga_putchar(char c)
{
    if (c == '\n') {
        vga_col = 0;
        vga_row++;
    } else if (c == '\b') {
        if (vga_col > 0) {
            vga_col--;
            VGA_BUFFER[vga_row * VGA_WIDTH + vga_col] = vga_entry(' ', vga_attr);
        }
    } else if (c == '\f') {
        vga_clear();
    } else if (c == '\r') {
        vga_col = 0;
    } else if (c == '\t') {
        /* Align to next 8-column boundary */
        vga_col = (vga_col + 8) & ~7u;
        if (vga_col >= VGA_WIDTH) {
            vga_col = 0;
            vga_row++;
        }
    } else {
        VGA_BUFFER[vga_row * VGA_WIDTH + vga_col] = vga_entry((unsigned char)c, vga_attr);
        vga_col++;
        if (vga_col >= VGA_WIDTH) {
            vga_col = 0;
            vga_row++;
        }
    }

    /* Scroll if we've passed the bottom */
    if (vga_row >= VGA_HEIGHT) {
        vga_scroll();
        vga_row = VGA_HEIGHT - 1;
    }
}

void vga_write(const char *str)
{
    for (size_t i = 0; str[i] != '\0'; i++)
        vga_putchar(str[i]);
}

void vga_write_at(const char *str, uint8_t color, size_t x, size_t y)
{
    size_t i = 0;
    while (str[i] != '\0' && (x + i) < VGA_WIDTH) {
        VGA_BUFFER[y * VGA_WIDTH + (x + i)] = vga_entry((unsigned char)str[i], color);
        i++;
    }
}
