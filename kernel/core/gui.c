/* ==========================================================================
 * AstraOS - GUI Window Manager Implementation
 * ==========================================================================
 * Compositing WM with Hyprland-inspired glassmorphism aesthetic.
 * Catppuccin Mocha palette, alpha-blended glass surfaces, rounded
 * corners, gradient desktop, and macOS-style window control dots.
 * ========================================================================== */

#include <kernel/gui.h>
#include <kernel/theme.h>
#include <drivers/framebuffer.h>
#include <drivers/mouse.h>
#include <drivers/keyboard.h>
#include <kernel/kheap.h>
#include <kernel/kstring.h>
#include <drivers/rtc.h>

static gui_window_t windows[GUI_MAX_WINDOWS];
static int focus_order[GUI_MAX_WINDOWS]; /* Z-order, [0] = topmost */
static int focus_count = 0;
static bool gui_active = false;
static bool needs_redraw = true;

/* Dragging state */
static bool dragging = false;
static int  drag_win = -1;
static int  drag_ox, drag_oy;

static int next_win_id = 1;

/* --- Internal helpers --- */

static gui_window_t *find_window(int id)
{
    for (int i = 0; i < GUI_MAX_WINDOWS; i++)
        if (windows[i].id == id && (windows[i].flags & WIN_VISIBLE))
            return &windows[i];
    return NULL;
}

static void draw_cursor(int x, int y)
{
    /* Arrow cursor bitmap: 0=transparent, 1=outline, 2=fill */
    static const uint8_t cursor[16][10] = {
        {1,0,0,0,0,0,0,0,0,0},
        {1,1,0,0,0,0,0,0,0,0},
        {1,2,1,0,0,0,0,0,0,0},
        {1,2,2,1,0,0,0,0,0,0},
        {1,2,2,2,1,0,0,0,0,0},
        {1,2,2,2,2,1,0,0,0,0},
        {1,2,2,2,2,2,1,0,0,0},
        {1,2,2,2,2,2,2,1,0,0},
        {1,2,2,2,2,2,2,2,1,0},
        {1,2,2,2,2,2,1,1,1,0},
        {1,2,2,1,2,2,1,0,0,0},
        {1,2,1,0,1,2,2,1,0,0},
        {1,1,0,0,1,2,2,1,0,0},
        {0,0,0,0,0,1,2,1,0,0},
        {0,0,0,0,0,1,2,1,0,0},
        {0,0,0,0,0,0,1,0,0,0},
    };

    /* Subtle drop shadow behind cursor */
    for (int r = 0; r < 16; r++)
        for (int c = 0; c < 10; c++) {
            if (cursor[r][c] != 0)
                fb_blend_pixel(x + c + 2, y + r + 2, RGBA(0,0,0,60));
        }

    /* Draw cursor */
    for (int r = 0; r < 16; r++)
        for (int c = 0; c < 10; c++) {
            if (cursor[r][c] == 1)
                fb_putpixel(x + c, y + r, COL_BLACK);
            else if (cursor[r][c] == 2)
                fb_putpixel(x + c, y + r, COL_WHITE);
        }
}

static void draw_taskbar(void)
{
    const fb_info_t *fb = fb_get_info();
    int tw = (int)fb->width;
    int ty = (int)fb->height - TASKBAR_H;

    const astra_theme_t *t = theme_current();

    /* Glass taskbar background */
    fb_fill_rect_alpha(0, ty, tw, TASKBAR_H,
                      RGBA(COL_R(t->taskbar), COL_G(t->taskbar), COL_B(t->taskbar), t->taskbar_alpha));
    /* Top highlight line for subtle glass edge */
    fb_fill_rect_alpha(0, ty, tw, 1,
                      RGBA(COL_R(t->accent), COL_G(t->accent), COL_B(t->accent), 40));

    /* "AstraOS" logo pill */
    fb_fill_rounded_rect(6, ty + 5, 84, TASKBAR_H - 10, 6,
                        RGBA(COL_R(t->accent), COL_G(t->accent), COL_B(t->accent), 50));
    fb_puts_transparent(16, ty + 10, "AstraOS", t->accent);

    /* Window buttons on taskbar */
    int bx = 100;
    for (int i = 0; i < focus_count && bx < tw - 140; i++) {
        gui_window_t *w = find_window(focus_order[i]);
        if (!w) continue;

        bool focused = (w->flags & WIN_FOCUSED) != 0;
        int bw = fb_text_width(w->title) + 20;
        if (bw < 64) bw = 64;
        if (bw > 160) bw = 160;

        /* Rounded pill button for each window */
        color_t btn_bg = focused
            ? RGBA(COL_R(t->accent), COL_G(t->accent), COL_B(t->accent), 60)
            : RGBA(COL_R(t->surface), COL_G(t->surface), COL_B(t->surface), 120);
        fb_fill_rounded_rect(bx, ty + 5, bw, TASKBAR_H - 10, 6, btn_bg);

        /* Active indicator dot */
        if (focused)
            fb_fill_circle(bx + bw / 2, ty + TASKBAR_H - 4, 2, t->accent);

        color_t fg = focused ? t->text : t->text_dim;
        fb_puts_transparent(bx + 10, ty + 10, w->title, fg);
        bx += bw + 6;
    }

    /* Clock on right - centered vertically */
    rtc_time_t rtc;
    rtc_read(&rtc);
    char clock[9];
    clock[0] = '0' + (char)(rtc.hour / 10);
    clock[1] = '0' + (char)(rtc.hour % 10);
    clock[2] = ':';
    clock[3] = '0' + (char)(rtc.minute / 10);
    clock[4] = '0' + (char)(rtc.minute % 10);
    clock[5] = ':';
    clock[6] = '0' + (char)(rtc.second / 10);
    clock[7] = '0' + (char)(rtc.second % 10);
    clock[8] = '\0';
    int cw = fb_text_width(clock);
    fb_puts_transparent(tw - cw - 16, ty + 10, clock, t->text);
}

static void draw_desktop(void)
{
    const fb_info_t *info = fb_get_info();
    int sw = (int)info->width;
    int sh = (int)info->height;

    /* Draw wallpaper from theme engine */
    theme_draw_wallpaper(sw, sh);

    /* Subtle subtitle */
    const char *sub = "Programming Environment";
    int subw = fb_text_width(sub);
    const astra_theme_t *t = theme_current();
    fb_puts_transparent((sw - subw) / 2, sh / 2 - 16, sub,
                       RGBA(COL_R(t->text), COL_G(t->text), COL_B(t->text), 35));
}

static void draw_window(gui_window_t *w)
{
    const astra_theme_t *t = theme_current();
    bool focused = (w->flags & WIN_FOCUSED) != 0;
    int r = 10; /* corner radius */

    /* Multi-layer shadow for depth */
    for (int i = 4; i > 0; i--) {
        uint32_t sa = (uint32_t)(focused ? 40 : 20) / (uint32_t)i;
        fb_fill_rect_alpha(w->x + i * 2, w->y + i * 2, w->w, w->h,
                          RGBA(0, 0, 0, sa));
    }

    /* Glass body - semi-transparent rounded rect */
    uint32_t body_a = focused ? (uint32_t)t->window_alpha : (uint32_t)t->window_alpha - 40;
    color_t body_bg = RGBA(COL_R(t->bg), COL_G(t->bg), COL_B(t->bg), body_a);
    fb_fill_rounded_rect(w->x, w->y, w->w, w->h, r, body_bg);

    /* Glowing border */
    color_t border = focused
        ? RGBA(COL_R(t->accent), COL_G(t->accent), COL_B(t->accent), 120)
        : RGBA(COL_R(t->border), COL_G(t->border), COL_B(t->border), 80);
    fb_rounded_rect(w->x, w->y, w->w, w->h, r, border);

    if (w->flags & WIN_TITLEBAR) {
        /* Title bar - slightly more opaque with glass surface */
        fb_fill_rect_alpha(w->x + 1, w->y + 1, w->w - 2, TITLEBAR_H - 1,
                          RGBA(COL_R(t->titlebar), COL_G(t->titlebar),
                               COL_B(t->titlebar), t->titlebar_alpha));
        /* Separator line */
        fb_fill_rect_alpha(w->x + 1, w->y + TITLEBAR_H, w->w - 2, 1,
                          RGBA(COL_R(t->border), COL_G(t->border), COL_B(t->border), 80));

        /* Window control dots (macOS-style) */
        int dot_y = w->y + TITLEBAR_H / 2;
        fb_fill_circle(w->x + 16, dot_y, 6, t->red);
        fb_fill_circle(w->x + 34, dot_y, 6, t->yellow);
        fb_fill_circle(w->x + 52, dot_y, 6, t->green);

        /* Title text - centered */
        int ttw = fb_text_width(w->title);
        int tx = w->x + (w->w - ttw) / 2;
        fb_puts_transparent(tx, w->y + 8,
                          w->title, focused ? t->text : t->text_dim);
    }

    /* Render content */
    if (w->on_render) {
        int cx = w->x + BORDER_W;
        int cy = w->y + (w->flags & WIN_TITLEBAR ? TITLEBAR_H + 1 : BORDER_W);
        int cw = w->w - 2 * BORDER_W;
        int ch = w->h - (w->flags & WIN_TITLEBAR ? TITLEBAR_H + BORDER_W + 1 : 2 * BORDER_W);
        w->on_render(w->id, cx, cy, cw, ch);
    }
}

static int window_at(int x, int y)
{
    /* Check in z-order (front to back) */
    for (int i = 0; i < focus_count; i++) {
        gui_window_t *w = find_window(focus_order[i]);
        if (!w) continue;
        if (x >= w->x && x < w->x + w->w &&
            y >= w->y && y < w->y + w->h)
            return w->id;
    }
    return -1;
}

static void set_focus(int id)
{
    /* Remove from current position in focus_order */
    int idx = -1;
    for (int i = 0; i < focus_count; i++) {
        if (focus_order[i] == id) { idx = i; break; }
    }
    if (idx < 0) return;

    /* Clear old focus */
    for (int i = 0; i < GUI_MAX_WINDOWS; i++)
        windows[i].flags &= ~WIN_FOCUSED;

    /* Move to front */
    for (int i = idx; i > 0; i--)
        focus_order[i] = focus_order[i - 1];
    focus_order[0] = id;

    gui_window_t *w = find_window(id);
    if (w) w->flags |= WIN_FOCUSED;
    needs_redraw = true;
}

static void handle_mouse(void)
{
    mouse_state_t ms = mouse_get_state();

    if (ms.moved || ms.clicked || ms.released)
        needs_redraw = true;

    if (ms.clicked && (ms.click_btn & MOUSE_LEFT)) {
        int wid = window_at(ms.x, ms.y);
        if (wid >= 0) {
            gui_window_t *w = find_window(wid);
            if (w) {
                /* Check close dot (centered at w->x+16, w->y+TITLEBAR_H/2, r=6) */
                if ((w->flags & WIN_CLOSABLE) && (w->flags & WIN_TITLEBAR)) {
                    int dx = ms.x - (w->x + 16);
                    int dy = ms.y - (w->y + TITLEBAR_H / 2);
                    if (dx * dx + dy * dy <= 8 * 8) {
                        gui_close_window(wid);
                        return;
                    }
                }

                set_focus(wid);

                /* Check if click is in title bar → start drag */
                if ((w->flags & WIN_MOVABLE) && (w->flags & WIN_TITLEBAR) &&
                    ms.y >= w->y && ms.y < w->y + TITLEBAR_H) {
                    dragging = true;
                    drag_win = wid;
                    drag_ox = ms.x - w->x;
                    drag_oy = ms.y - w->y;
                } else {
                    /* Content click */
                    if (w->on_click) {
                        int cx = w->x + BORDER_W;
                        int cy = w->y + (w->flags & WIN_TITLEBAR ? TITLEBAR_H + 1 : BORDER_W);
                        w->on_click(wid, ms.x - cx, ms.y - cy, ms.click_btn);
                    }
                }
            }
        }
    }

    if (ms.released && (ms.release_btn & MOUSE_LEFT)) {
        dragging = false;
        drag_win = -1;
    }

    if (dragging && ms.moved) {
        gui_window_t *w = find_window(drag_win);
        if (w) {
            w->x = ms.x - drag_ox;
            w->y = ms.y - drag_oy;
            needs_redraw = true;
        }
    }
}

static void handle_keyboard(void)
{
    int c = keyboard_getchar();
    if (c < 0) return;

    /* Send to focused window */
    if (focus_count > 0) {
        gui_window_t *w = find_window(focus_order[0]);
        if (w && w->on_key) {
            w->on_key(w->id, c);
            needs_redraw = true;
        }
    }
}

/* --- Public API --- */

void gui_init(void)
{
    theme_init();
    kmemset(windows, 0, sizeof(windows));
    focus_count = 0;
    gui_active = true;
    needs_redraw = true;
}

void gui_tick(void)
{
    if (!gui_active) return;

    handle_mouse();
    handle_keyboard();

    if (needs_redraw) {
        needs_redraw = false;
        draw_desktop();

        /* Draw windows back to front */
        for (int i = focus_count - 1; i >= 0; i--) {
            gui_window_t *w = find_window(focus_order[i]);
            if (w) draw_window(w);
        }

        draw_taskbar();

        mouse_state_t ms = mouse_peek_state();
        draw_cursor(ms.x, ms.y);

        fb_swap();
    }
}

int gui_create_window(const char *title, int x, int y, int w, int h, uint32_t flags)
{
    int slot = -1;
    for (int i = 0; i < GUI_MAX_WINDOWS; i++) {
        if (windows[i].id == 0) { slot = i; break; }
    }
    if (slot < 0) return -1;

    gui_window_t *win = &windows[slot];
    win->id = next_win_id++;
    kstrncpy(win->title, title, 63);
    win->title[63] = '\0';
    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;
    win->flags = flags;
    win->on_render = NULL;
    win->on_key = NULL;
    win->on_click = NULL;
    win->userdata = NULL;
    win->dirty = true;

    /* Add to focus order (front) */
    if (focus_count < GUI_MAX_WINDOWS) {
        for (int i = focus_count; i > 0; i--)
            focus_order[i] = focus_order[i - 1];
        focus_order[0] = win->id;
        focus_count++;
    }

    /* Set focus */
    for (int i = 0; i < GUI_MAX_WINDOWS; i++)
        windows[i].flags &= ~WIN_FOCUSED;
    win->flags |= WIN_FOCUSED;

    needs_redraw = true;
    return win->id;
}

void gui_close_window(int id)
{
    for (int i = 0; i < GUI_MAX_WINDOWS; i++) {
        if (windows[i].id == id) {
            windows[i].id = 0;
            windows[i].flags = 0;
            break;
        }
    }

    /* Remove from focus order */
    int idx = -1;
    for (int i = 0; i < focus_count; i++) {
        if (focus_order[i] == id) { idx = i; break; }
    }
    if (idx >= 0) {
        for (int i = idx; i < focus_count - 1; i++)
            focus_order[i] = focus_order[i + 1];
        focus_count--;
    }

    /* Focus next window if any */
    if (focus_count > 0) {
        gui_window_t *w = find_window(focus_order[0]);
        if (w) w->flags |= WIN_FOCUSED;
    }

    needs_redraw = true;
}

gui_window_t *gui_get_window(int id)
{
    return find_window(id);
}

void gui_set_render(int id, win_render_fn fn)
{
    gui_window_t *w = find_window(id);
    if (w) w->on_render = fn;
}

void gui_set_key_handler(int id, win_key_fn fn)
{
    gui_window_t *w = find_window(id);
    if (w) w->on_key = fn;
}

void gui_set_click_handler(int id, win_click_fn fn)
{
    gui_window_t *w = find_window(id);
    if (w) w->on_click = fn;
}

void gui_set_userdata(int id, void *data)
{
    gui_window_t *w = find_window(id);
    if (w) w->userdata = data;
}

void gui_invalidate(int id)
{
    gui_window_t *w = find_window(id);
    if (w) w->dirty = true;
    needs_redraw = true;
}

void gui_focus_window(int id)
{
    set_focus(id);
}

void gui_content_rect(int id, int *cx, int *cy, int *cw, int *ch)
{
    gui_window_t *w = find_window(id);
    if (!w) return;
    *cx = w->x + BORDER_W;
    *cy = w->y + (w->flags & WIN_TITLEBAR ? TITLEBAR_H + 1 : BORDER_W);
    *cw = w->w - 2 * BORDER_W;
    *ch = w->h - (w->flags & WIN_TITLEBAR ? TITLEBAR_H + BORDER_W + 1 : 2 * BORDER_W);
}

void gui_draw_text(int win_id, int rx, int ry, const char *text, color_t fg)
{
    gui_window_t *w = find_window(win_id);
    if (!w) return;
    int cx = w->x + BORDER_W + rx;
    int cy = w->y + (w->flags & WIN_TITLEBAR ? TITLEBAR_H + 1 : BORDER_W) + ry;
    fb_puts_transparent(cx, cy, text, fg);
}

void gui_draw_rect(int win_id, int rx, int ry, int rw, int rh, color_t color)
{
    gui_window_t *w = find_window(win_id);
    if (!w) return;
    int cx = w->x + BORDER_W + rx;
    int cy = w->y + (w->flags & WIN_TITLEBAR ? TITLEBAR_H + 1 : BORDER_W) + ry;
    fb_rect(cx, cy, rw, rh, color);
}

void gui_fill_rect(int win_id, int rx, int ry, int rw, int rh, color_t color)
{
    gui_window_t *w = find_window(win_id);
    if (!w) return;
    int cx = w->x + BORDER_W + rx;
    int cy = w->y + (w->flags & WIN_TITLEBAR ? TITLEBAR_H + 1 : BORDER_W) + ry;
    fb_fill_rect(cx, cy, rw, rh, color);
}

void gui_redraw_all(void)
{
    needs_redraw = true;
}

bool gui_is_active(void)
{
    return gui_active;
}
