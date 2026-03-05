/* ==========================================================================
 * AstraOS - Framebuffer Graphics Driver
 * ==========================================================================
 * VESA linear framebuffer. Supports 32-bit and 24-bit color modes.
 * Includes double buffering, rectangle drawing, and bitmap font rendering.
 * ========================================================================== */

#include <drivers/framebuffer.h>
#include <kernel/multiboot.h>
#include <kernel/kheap.h>
#include <kernel/kstring.h>
#include <kernel/vmm.h>

/* Font data (defined in font8x16.c) */
extern const uint8_t font8x16[128][16];

static fb_info_t fb;
static uint32_t *backbuf = NULL;
static bool double_buffered = false;

int fb_init(void *mboot_ptr)
{
    multiboot_info_t *mboot = (multiboot_info_t *)mboot_ptr;

    if (!(mboot->flags & MULTIBOOT_FLAG_FB))
        return -1;

    fb.addr   = (uint32_t *)(uint32_t)mboot->framebuffer_addr;
    fb.width  = mboot->framebuffer_width;
    fb.height = mboot->framebuffer_height;
    fb.pitch  = mboot->framebuffer_pitch;
    fb.bpp    = mboot->framebuffer_bpp;

    /* Identity-map the framebuffer region so we can write to it.
     * The framebuffer is typically at a high physical address (e.g. 0xFD000000).
     * We need to map enough pages for width * height * 4 bytes. */
    uint32_t fb_size = fb.pitch * fb.height;
    uint32_t fb_phys = (uint32_t)fb.addr;
    uint32_t pages = (fb_size + 0xFFF) / 0x1000;

    for (uint32_t i = 0; i < pages; i++) {
        uint32_t addr = fb_phys + i * 0x1000;
        vmm_identity_map(addr);
    }

    return 0;
}

const fb_info_t *fb_get_info(void)
{
    return &fb;
}

void fb_set_double_buffered(bool enable)
{
    if (enable && !backbuf) {
        backbuf = (uint32_t *)kmalloc(fb.pitch * fb.height);
        if (backbuf)
            kmemset(backbuf, 0, fb.pitch * fb.height);
    }
    double_buffered = enable && (backbuf != NULL);
}

static inline uint32_t *fb_target(void)
{
    return double_buffered ? backbuf : fb.addr;
}

void fb_swap(void)
{
    if (double_buffered && backbuf)
        kmemcpy(fb.addr, backbuf, fb.pitch * fb.height);
}

void fb_putpixel(int x, int y, color_t color)
{
    if (x < 0 || y < 0 || (uint32_t)x >= fb.width || (uint32_t)y >= fb.height)
        return;
    uint32_t *target = fb_target();
    uint32_t *row = (uint32_t *)((uint8_t *)target + y * fb.pitch);
    row[x] = color;
}

void fb_fill_rect(int x, int y, int w, int h, color_t color)
{
    /* Clip */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)fb.width)  w = (int)fb.width - x;
    if (y + h > (int)fb.height) h = (int)fb.height - y;
    if (w <= 0 || h <= 0) return;

    uint32_t *target = fb_target();
    for (int row = 0; row < h; row++) {
        uint32_t *p = (uint32_t *)((uint8_t *)target + (y + row) * fb.pitch) + x;
        for (int col = 0; col < w; col++)
            p[col] = color;
    }
}

void fb_hline(int x, int y, int w, color_t color)
{
    fb_fill_rect(x, y, w, 1, color);
}

void fb_vline(int x, int y, int h, color_t color)
{
    fb_fill_rect(x, y, 1, h, color);
}

void fb_rect(int x, int y, int w, int h, color_t color)
{
    fb_hline(x, y, w, color);
    fb_hline(x, y + h - 1, w, color);
    fb_vline(x, y, h, color);
    fb_vline(x + w - 1, y, h, color);
}

void fb_clear(color_t color)
{
    fb_fill_rect(0, 0, (int)fb.width, (int)fb.height, color);
}

void fb_putchar(int x, int y, char c, color_t fg, color_t bg)
{
    uint8_t idx = (uint8_t)c;
    if (idx > 127) idx = '?';
    const uint8_t *glyph = font8x16[idx];

    uint32_t *target = fb_target();
    for (int row = 0; row < FONT_H; row++) {
        int py = y + row;
        if (py < 0 || (uint32_t)py >= fb.height) continue;
        uint32_t *p = (uint32_t *)((uint8_t *)target + py * fb.pitch);
        uint8_t bits = glyph[row];
        for (int col = 0; col < FONT_W; col++) {
            int px = x + col;
            if (px < 0 || (uint32_t)px >= fb.width) continue;
            p[px] = (bits & (0x80 >> col)) ? fg : bg;
        }
    }
}

void fb_puts(int x, int y, const char *s, color_t fg, color_t bg)
{
    while (*s) {
        if (*s == '\n') {
            y += FONT_H;
            x = 0;
        } else {
            fb_putchar(x, y, *s, fg, bg);
            x += FONT_W;
        }
        s++;
    }
}

void fb_puts_transparent(int x, int y, const char *s, color_t fg)
{
    uint32_t *target = fb_target();
    while (*s) {
        if (*s == '\n') {
            y += FONT_H;
            x = 0;
            s++;
            continue;
        }
        uint8_t idx = (uint8_t)*s;
        if (idx > 127) idx = '?';
        const uint8_t *glyph = font8x16[idx];
        for (int row = 0; row < FONT_H; row++) {
            int py = y + row;
            if (py < 0 || (uint32_t)py >= fb.height) continue;
            uint32_t *p = (uint32_t *)((uint8_t *)target + py * fb.pitch);
            uint8_t bits = glyph[row];
            for (int col = 0; col < FONT_W; col++) {
                int px = x + col;
                if (px >= 0 && (uint32_t)px < fb.width && (bits & (0x80 >> col)))
                    p[px] = fg;
            }
        }
        x += FONT_W;
        s++;
    }
}

int fb_text_width(const char *s)
{
    int w = 0;
    while (*s++) w += FONT_W;
    return w;
}

void fb_blit(int dx, int dy, int w, int h, const uint32_t *src, int src_pitch)
{
    uint32_t *target = fb_target();
    for (int row = 0; row < h; row++) {
        int py = dy + row;
        if (py < 0 || (uint32_t)py >= fb.height) continue;
        uint32_t *dst_row = (uint32_t *)((uint8_t *)target + py * fb.pitch) + dx;
        const uint32_t *src_row = (const uint32_t *)((const uint8_t *)src + row * src_pitch);
        int cw = w;
        int sx = 0;
        if (dx < 0) { sx = -dx; cw += dx; }
        if (dx + cw > (int)fb.width) cw = (int)fb.width - dx;
        if (cw > 0)
            kmemcpy(dst_row + (sx > 0 ? sx : 0), src_row + sx, (uint32_t)cw * 4);
    }
}

void fb_scroll_region(int x, int y, int w, int h, int n, color_t fill)
{
    if (n <= 0 || n >= h) {
        fb_fill_rect(x, y, w, h, fill);
        return;
    }
    uint32_t *target = fb_target();
    for (int row = 0; row < h - n; row++) {
        uint32_t *dst = (uint32_t *)((uint8_t *)target + (y + row) * fb.pitch) + x;
        uint32_t *src = (uint32_t *)((uint8_t *)target + (y + row + n) * fb.pitch) + x;
        kmemcpy(dst, src, (uint32_t)w * 4);
    }
    fb_fill_rect(x, y + h - n, w, n, fill);
}
