/* ==========================================================================
 * AstraOS - Framebuffer Graphics Driver
 * ==========================================================================
 * VESA linear framebuffer driver for pixel-based graphics.
 * Initialized from multiboot VBE information.
 * ========================================================================== */

#ifndef _ASTRA_DRIVERS_FRAMEBUFFER_H
#define _ASTRA_DRIVERS_FRAMEBUFFER_H

#include <kernel/types.h>

/* Color as 32-bit ARGB */
typedef uint32_t color_t;

/* Color macros */
#define RGB(r,g,b)     ((color_t)(0xFF000000 | ((r)<<16) | ((g)<<8) | (b)))
#define RGBA(r,g,b,a)  ((color_t)(((a)<<24) | ((r)<<16) | ((g)<<8) | (b)))

/* Standard palette */
#define COL_BLACK       RGB(0,0,0)
#define COL_WHITE       RGB(255,255,255)
#define COL_BG_DARK     RGB(30,30,46)
#define COL_BG_MED      RGB(45,45,65)
#define COL_BG_LIGHT    RGB(60,60,85)
#define COL_ACCENT      RGB(137,180,250)
#define COL_ACCENT2     RGB(166,227,161)
#define COL_TEXT         RGB(205,214,244)
#define COL_TEXT_DIM     RGB(147,153,178)
#define COL_RED         RGB(243,139,168)
#define COL_GREEN       RGB(166,227,161)
#define COL_YELLOW      RGB(249,226,175)
#define COL_BLUE        RGB(137,180,250)
#define COL_PURPLE      RGB(203,166,247)
#define COL_ORANGE      RGB(250,179,135)
#define COL_SURFACE     RGB(49,50,68)
#define COL_OVERLAY     RGB(108,112,134)
#define COL_BORDER      RGB(88,91,112)

/* Framebuffer info */
typedef struct {
    uint32_t *addr;     /* Linear framebuffer address */
    uint32_t  width;
    uint32_t  height;
    uint32_t  pitch;    /* Bytes per scanline */
    uint8_t   bpp;      /* Bits per pixel */
} fb_info_t;

/* Initialize framebuffer from multiboot info */
int fb_init(void *mboot_info);

/* Get framebuffer info */
const fb_info_t *fb_get_info(void);

/* Single pixel */
void fb_putpixel(int x, int y, color_t color);

/* Filled rectangle */
void fb_fill_rect(int x, int y, int w, int h, color_t color);

/* Horizontal line (fast) */
void fb_hline(int x, int y, int w, color_t color);

/* Vertical line */
void fb_vline(int x, int y, int h, color_t color);

/* Rectangle outline */
void fb_rect(int x, int y, int w, int h, color_t color);

/* Clear entire screen */
void fb_clear(color_t color);

/* Draw character at pixel position (8x16 bitmap font) */
void fb_putchar(int x, int y, char c, color_t fg, color_t bg);

/* Draw string at pixel position */
void fb_puts(int x, int y, const char *s, color_t fg, color_t bg);

/* Draw string with no background (transparent) */
void fb_puts_transparent(int x, int y, const char *s, color_t fg);

/* Measure string width in pixels */
int fb_text_width(const char *s);

/* Font dimensions */
#define FONT_W 8
#define FONT_H 16

/* Copy region (blit) */
void fb_blit(int dx, int dy, int w, int h, const uint32_t *src, int src_pitch);

/* Scroll a region up by n pixels */
void fb_scroll_region(int x, int y, int w, int h, int n, color_t fill);

/* Double buffering */
void fb_swap(void);     /* Copy back buffer to screen */
void fb_set_double_buffered(bool enable);

#endif /* _ASTRA_DRIVERS_FRAMEBUFFER_H */
