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

void fb_memset32(uint32_t *dst, uint32_t val, uint32_t count)
{
#ifdef __i386__
    __asm__ volatile (
        "rep stosl"
        : "+D"(dst), "+c"(count)
        : "a"(val)
        : "memory"
    );
#else
    for (uint32_t i = 0; i < count; i++)
        dst[i] = val;
#endif
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
        fb_memset32(p, color, (uint32_t)w);
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

/* ========================================================================
 * Alpha blending & compositing functions
 * ======================================================================== */

void fb_blend_pixel(int x, int y, color_t color)
{
    if (x < 0 || y < 0 || (uint32_t)x >= fb.width || (uint32_t)y >= fb.height)
        return;

    uint32_t alpha = COL_A(color);
    if (alpha == 0xFF) { fb_putpixel(x, y, color); return; }
    if (alpha == 0) return;

    uint32_t *target = fb_target();
    uint32_t *row = (uint32_t *)((uint8_t *)target + y * fb.pitch);
    uint32_t bg = row[x];

    uint32_t inv = 255 - alpha;
    uint32_t r = (COL_R(color) * alpha + COL_R(bg) * inv) / 255;
    uint32_t g = (COL_G(color) * alpha + COL_G(bg) * inv) / 255;
    uint32_t b = (COL_B(color) * alpha + COL_B(bg) * inv) / 255;
    row[x] = RGB(r, g, b);
}

void fb_fill_rect_alpha(int x, int y, int w, int h, color_t color)
{
    uint32_t alpha = COL_A(color);
    if (alpha == 0) return;
    if (alpha == 0xFF) { fb_fill_rect(x, y, w, h, color); return; }

    /* Clip */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)fb.width)  w = (int)fb.width - x;
    if (y + h > (int)fb.height) h = (int)fb.height - y;
    if (w <= 0 || h <= 0) return;

    uint32_t inv = 255 - alpha;
    uint32_t sr = COL_R(color) * alpha;
    uint32_t sg = COL_G(color) * alpha;
    uint32_t sb = COL_B(color) * alpha;

    uint32_t *target = fb_target();
    for (int row = 0; row < h; row++) {
        uint32_t *p = (uint32_t *)((uint8_t *)target + (y + row) * fb.pitch) + x;
        for (int col = 0; col < w; col++) {
            uint32_t bg = p[col];
            uint32_t r = (sr + COL_R(bg) * inv) / 255;
            uint32_t g = (sg + COL_G(bg) * inv) / 255;
            uint32_t b = (sb + COL_B(bg) * inv) / 255;
            p[col] = RGB(r, g, b);
        }
    }
}

/* Check if point (px,py) is inside a rounded corner.
 * (cx,cy) is the corner center. Returns true if inside. */
static bool _in_rounded_corner(int px, int py, int cx, int cy, int r)
{
    int dx = px - cx;
    int dy = py - cy;
    return (dx * dx + dy * dy) <= (r * r);
}

void fb_fill_rounded_rect(int x, int y, int w, int h, int radius, color_t color)
{
    if (w <= 0 || h <= 0) return;
    if (radius <= 0) {
        if (COL_A(color) == 0xFF)
            fb_fill_rect(x, y, w, h, color);
        else
            fb_fill_rect_alpha(x, y, w, h, color);
        return;
    }
    /* Clamp radius to half the smaller dimension */
    if (radius > w / 2) radius = w / 2;
    if (radius > h / 2) radius = h / 2;

    bool use_alpha = (COL_A(color) < 0xFF);

    /* Corner centers */
    int tl_cx = x + radius, tl_cy = y + radius;         /* top-left */
    int tr_cx = x + w - 1 - radius, tr_cy = y + radius;  /* top-right */
    int bl_cx = x + radius, bl_cy = y + h - 1 - radius;  /* bottom-left */
    int br_cx = x + w - 1 - radius, br_cy = y + h - 1 - radius; /* bottom-right */

    for (int py = y; py < y + h; py++) {
        for (int px = x; px < x + w; px++) {
            bool draw = true;
            /* Top-left corner */
            if (px < tl_cx && py < tl_cy)
                draw = _in_rounded_corner(px, py, tl_cx, tl_cy, radius);
            /* Top-right corner */
            else if (px > tr_cx && py < tr_cy)
                draw = _in_rounded_corner(px, py, tr_cx, tr_cy, radius);
            /* Bottom-left corner */
            else if (px < bl_cx && py > bl_cy)
                draw = _in_rounded_corner(px, py, bl_cx, bl_cy, radius);
            /* Bottom-right corner */
            else if (px > br_cx && py > br_cy)
                draw = _in_rounded_corner(px, py, br_cx, br_cy, radius);

            if (draw) {
                if (use_alpha)
                    fb_blend_pixel(px, py, color);
                else
                    fb_putpixel(px, py, color);
            }
        }
    }
}

void fb_rounded_rect(int x, int y, int w, int h, int radius, color_t color)
{
    if (w <= 0 || h <= 0) return;
    if (radius <= 0) { fb_rect(x, y, w, h, color); return; }
    if (radius > w / 2) radius = w / 2;
    if (radius > h / 2) radius = h / 2;

    bool use_alpha = (COL_A(color) < 0xFF);

    /* Draw straight edges (excluding corners) */
    for (int px = x + radius; px <= x + w - 1 - radius; px++) {
        if (use_alpha) {
            fb_blend_pixel(px, y, color);
            fb_blend_pixel(px, y + h - 1, color);
        } else {
            fb_putpixel(px, y, color);
            fb_putpixel(px, y + h - 1, color);
        }
    }
    for (int py = y + radius; py <= y + h - 1 - radius; py++) {
        if (use_alpha) {
            fb_blend_pixel(x, py, color);
            fb_blend_pixel(x + w - 1, py, color);
        } else {
            fb_putpixel(x, py, color);
            fb_putpixel(x + w - 1, py, color);
        }
    }

    /* Draw rounded corners using midpoint circle algorithm */
    int cx, cy;
    int ix = radius, iy = 0;
    int err = 1 - radius;

    while (ix >= iy) {
        /* Top-left corner */
        cx = x + radius; cy = y + radius;
        if (use_alpha) {
            fb_blend_pixel(cx - ix, cy - iy, color);
            fb_blend_pixel(cx - iy, cy - ix, color);
        } else {
            fb_putpixel(cx - ix, cy - iy, color);
            fb_putpixel(cx - iy, cy - ix, color);
        }
        /* Top-right corner */
        cx = x + w - 1 - radius; cy = y + radius;
        if (use_alpha) {
            fb_blend_pixel(cx + ix, cy - iy, color);
            fb_blend_pixel(cx + iy, cy - ix, color);
        } else {
            fb_putpixel(cx + ix, cy - iy, color);
            fb_putpixel(cx + iy, cy - ix, color);
        }
        /* Bottom-left corner */
        cx = x + radius; cy = y + h - 1 - radius;
        if (use_alpha) {
            fb_blend_pixel(cx - ix, cy + iy, color);
            fb_blend_pixel(cx - iy, cy + ix, color);
        } else {
            fb_putpixel(cx - ix, cy + iy, color);
            fb_putpixel(cx - iy, cy + ix, color);
        }
        /* Bottom-right corner */
        cx = x + w - 1 - radius; cy = y + h - 1 - radius;
        if (use_alpha) {
            fb_blend_pixel(cx + ix, cy + iy, color);
            fb_blend_pixel(cx + iy, cy + ix, color);
        } else {
            fb_putpixel(cx + ix, cy + iy, color);
            fb_putpixel(cx + iy, cy + ix, color);
        }

        iy++;
        if (err < 0) {
            err += 2 * iy + 1;
        } else {
            ix--;
            err += 2 * (iy - ix) + 1;
        }
    }
}

void fb_gradient_h(int x, int y, int w, int h, color_t left, color_t right)
{
    if (w <= 0 || h <= 0) return;

    int lr = (int)COL_R(left),  lg = (int)COL_G(left),  lb = (int)COL_B(left),  la = (int)COL_A(left);
    int rr = (int)COL_R(right), rg = (int)COL_G(right), rb = (int)COL_B(right), ra = (int)COL_A(right);

    for (int col = 0; col < w; col++) {
        /* Linear interpolation: val = left + (right - left) * col / (w - 1) */
        int denom = (w > 1) ? (w - 1) : 1;
        uint32_t r = (uint32_t)(lr + (rr - lr) * col / denom);
        uint32_t g = (uint32_t)(lg + (rg - lg) * col / denom);
        uint32_t b = (uint32_t)(lb + (rb - lb) * col / denom);
        uint32_t a = (uint32_t)(la + (ra - la) * col / denom);
        color_t c = RGBA(r, g, b, a);

        if (a == 0xFF) {
            for (int row = 0; row < h; row++)
                fb_putpixel(x + col, y + row, c);
        } else {
            for (int row = 0; row < h; row++)
                fb_blend_pixel(x + col, y + row, c);
        }
    }
}

void fb_gradient_v(int x, int y, int w, int h, color_t top, color_t bottom)
{
    if (w <= 0 || h <= 0) return;

    int tr = (int)COL_R(top),    tg = (int)COL_G(top),    tb = (int)COL_B(top),    ta = (int)COL_A(top);
    int br = (int)COL_R(bottom), bg = (int)COL_G(bottom), bb = (int)COL_B(bottom), ba = (int)COL_A(bottom);

    for (int row = 0; row < h; row++) {
        int denom = (h > 1) ? (h - 1) : 1;
        uint32_t r = (uint32_t)(tr + (br - tr) * row / denom);
        uint32_t g = (uint32_t)(tg + (bg - tg) * row / denom);
        uint32_t b = (uint32_t)(tb + (bb - tb) * row / denom);
        uint32_t a = (uint32_t)(ta + (ba - ta) * row / denom);
        color_t c = RGBA(r, g, b, a);

        if (a == 0xFF) {
            /* Fast path: use fb_fill_rect for a single row */
            fb_fill_rect(x, y + row, w, 1, c);
        } else {
            fb_fill_rect_alpha(x, y + row, w, 1, c);
        }
    }
}

void fb_fill_circle(int cx, int cy, int r, color_t color)
{
    if (r <= 0) return;
    bool use_alpha = (COL_A(color) < 0xFF);
    int r2 = r * r;

    for (int dy = -r; dy <= r; dy++) {
        /* Compute horizontal span for this row */
        int dx_max_sq = r2 - dy * dy;
        if (dx_max_sq < 0) continue;

        /* Integer square root approximation for span width */
        int dx_max = 0;
        while ((dx_max + 1) * (dx_max + 1) <= dx_max_sq)
            dx_max++;

        for (int dx = -dx_max; dx <= dx_max; dx++) {
            if (use_alpha)
                fb_blend_pixel(cx + dx, cy + dy, color);
            else
                fb_putpixel(cx + dx, cy + dy, color);
        }
    }
}

void fb_putchar_alpha(int x, int y, char c, color_t fg, color_t bg_color)
{
    uint8_t idx = (uint8_t)c;
    if (idx > 127) idx = '?';
    const uint8_t *glyph = font8x16[idx];
    uint32_t bg_alpha = COL_A(bg_color);

    uint32_t *target = fb_target();
    for (int row = 0; row < FONT_H; row++) {
        int py = y + row;
        if (py < 0 || (uint32_t)py >= fb.height) continue;
        uint32_t *p = (uint32_t *)((uint8_t *)target + py * fb.pitch);
        uint8_t bits = glyph[row];
        for (int col = 0; col < FONT_W; col++) {
            int px = x + col;
            if (px < 0 || (uint32_t)px >= fb.width) continue;
            if (bits & (0x80 >> col)) {
                /* Foreground pixel - always opaque */
                p[px] = fg;
            } else if (bg_alpha == 0xFF) {
                /* Opaque background */
                p[px] = bg_color;
            } else if (bg_alpha > 0) {
                /* Semi-transparent background - blend */
                uint32_t inv = 255 - bg_alpha;
                uint32_t existing = p[px];
                uint32_t r = (COL_R(bg_color) * bg_alpha + COL_R(existing) * inv) / 255;
                uint32_t g = (COL_G(bg_color) * bg_alpha + COL_G(existing) * inv) / 255;
                uint32_t b = (COL_B(bg_color) * bg_alpha + COL_B(existing) * inv) / 255;
                p[px] = RGB(r, g, b);
            }
            /* bg_alpha == 0: fully transparent background, leave pixel as-is */
        }
    }
}

void fb_puts_alpha(int x, int y, const char *s, color_t fg, color_t bg_color)
{
    while (*s) {
        if (*s == '\n') {
            y += FONT_H;
            x = 0;
        } else {
            fb_putchar_alpha(x, y, *s, fg, bg_color);
            x += FONT_W;
        }
        s++;
    }
}
