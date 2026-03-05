/* ==========================================================================
 * AstraOS - VGA Text Mode Driver Interface
 * ==========================================================================
 * Standard 80x25 text mode (mode 3) VGA driver.
 * The VGA text buffer lives at physical address 0xB8000.
 * Each character cell is 2 bytes: [character][attribute].
 *
 * This driver is intentionally simple — it will be replaced by a proper
 * framebuffer/console subsystem in later phases.
 * ========================================================================== */

#ifndef _ASTRA_DRIVERS_VGA_H
#define _ASTRA_DRIVERS_VGA_H

#include <kernel/types.h>

/* VGA text mode dimensions */
#define VGA_WIDTH   80
#define VGA_HEIGHT  25

/* VGA color constants (4-bit) */
typedef enum {
    VGA_COLOR_BLACK         = 0,
    VGA_COLOR_BLUE          = 1,
    VGA_COLOR_GREEN         = 2,
    VGA_COLOR_CYAN          = 3,
    VGA_COLOR_RED           = 4,
    VGA_COLOR_MAGENTA       = 5,
    VGA_COLOR_BROWN         = 6,
    VGA_COLOR_LIGHT_GREY    = 7,
    VGA_COLOR_DARK_GREY     = 8,
    VGA_COLOR_LIGHT_BLUE    = 9,
    VGA_COLOR_LIGHT_GREEN   = 10,
    VGA_COLOR_LIGHT_CYAN    = 11,
    VGA_COLOR_LIGHT_RED     = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_YELLOW        = 14,
    VGA_COLOR_WHITE         = 15,
} vga_color_t;

/* Combine foreground and background into a single attribute byte */
static inline uint8_t vga_entry_color(vga_color_t fg, vga_color_t bg)
{
    return (uint8_t)(fg | (bg << 4));
}

/* Combine a character and attribute into a VGA cell value */
static inline uint16_t vga_entry(unsigned char c, uint8_t color)
{
    return (uint16_t)c | ((uint16_t)color << 8);
}

/* Driver API */
void vga_init(void);
void vga_set_color(uint8_t color);
void vga_putchar(char c);
void vga_write(const char *str);
void vga_write_at(const char *str, uint8_t color, size_t x, size_t y);
void vga_clear(void);

#endif /* _ASTRA_DRIVERS_VGA_H */
